#include "PhoswichProcessor.hpp"
#include "Structures.h"
#include "MapFile.hpp"
#include "Plotter.hpp"

#include "TGraph.h"
#include "TFitResult.h"
#include "TFitResultPtr.h"

/// Set the fit parameters for the current event.
bool PhoswichProcessor::SetFitParameters(ChannelEvent *event_, MapEntry *entry_){
	if(!event_ || !entry_){ return false; }

	// Set the initial parameters of the fast pulse.
	fast_x1 = event_->max_index - fitting_low;
	fast_x2 = event_->max_index + fitting_high;

	fitting_func->SetRange((double)fast_x1, (double)fast_x2);
	fitting_func->SetParameter(0, 5.571827*event_->maximum - 0.9336001); // Constant
	fitting_func->SetParameter(1, event_->max_index); // MPV
	fitting_func->SetParameter(2, 1.65004); // Sigma
	
	// Compute the trace qdc of the slow component of the pulse.
	slow_qdc = event_->IntegratePulse(event_->max_index + fitting_low2, event_->max_index + fitting_high2);
	
	return true;
}
	
/// Fit a single trace.
bool PhoswichProcessor::FitPulse(TGraph *trace_, float &phase){
	if(!trace_){ return false; }
	
	// Fit the fast pulse.
	fit_result = trace_->Fit(fitting_func, "SQRE");
	
	fast_A = fitting_func->GetParameter(0);
	fast_MPV = fitting_func->GetParameter(1);
	fast_Sigma = fitting_func->GetParameter(2);
	fast_chi2 = fit_result->Chi2()/fit_result->Ndf();
	fast_qdc = fitting_func->Integral(fast_x1, fast_x2);

	// Compute the phase by subtracting the pulse HWHM from the most-probable-value.
	phase = fitting_func->GetParameter(1) - 1.17741*fitting_func->GetParameter(2);
	
	return true;
}

/// Process all individual events.
bool PhoswichProcessor::HandleEvents(){
	if(!init){ return false; }

	ChannelEvent *current_event;

	for(std::deque<ChannelEventPair*>::iterator iter = events.begin(); iter != events.end(); iter++){
		current_event = (*iter)->event;
	
		// Check that the time and energy values are valid
		if(!current_event->valid_chan){ continue; }

		// Fill all diagnostic histograms.
		fast_energy_1d->Fill(fast_qdc);
		slow_energy_1d->Fill(slow_qdc);
		energy_2d->Fill(slow_qdc, fast_qdc);
		MPV_1d->Fill(fast_MPV);
	
		// Fill the values into the root tree.
		structure.Append(current_event->time, fast_qdc, slow_qdc, fast_A, fast_MPV, fast_Sigma, fast_chi2);
		
		// Copy the trace to the output file.
		if(write_waveform){
			waveform.Append((int*)current_event->yvals, current_event->size);
		}
		
		good_events++;
	}
	return true;
}

PhoswichProcessor::PhoswichProcessor(MapFile *map_) : Processor("Phoswich", "phoswich", map_){
	fitting_func = new TF1("f_fast", "landau", 0, 1);
	
	fitting_low = 5;
	fitting_high = 8;
	fitting_low2 = 15;
	fitting_high2 = 100;
	
	root_structure = (Structure*)&structure;
	root_waveform = (Waveform*)&waveform;
	
	fast_energy_1d = new Plotter("phoswich_h1", "Phoswich Fast LR", "", "Light Response (a.u.)", 200, 0, 20000);
	slow_energy_1d = new Plotter("phoswich_h2", "Phoswich Slow LR", "", "Light Response (a.u.)", 200, 0, 20000);
	energy_2d = new Plotter("phoswich_h3", "Phoswich Fast LR vs. Slow", "COLZ", "Slow Light Response (a.u.)", 200, 0, 20000, "Fast Light Response (a.u.)", 200, 0, 20000);
	MPV_1d = new Plotter("phoswich_h4", "Phoswich Most-Probable Value", "", "MPV (ns)", 100, 0, 100);
}

PhoswichProcessor::~PhoswichProcessor(){ 
	delete fast_energy_1d;
	delete slow_energy_1d;
	delete energy_2d;
	delete MPV_1d;
}

void PhoswichProcessor::GetHists(std::vector<Plotter*> &plots_){
	plots_.push_back(fast_energy_1d);
	plots_.push_back(slow_energy_1d);
	plots_.push_back(energy_2d);
	plots_.push_back(MPV_1d);
}