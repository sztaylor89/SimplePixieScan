#include <iostream>

#include "Processor.hpp"
#include "Plotter.hpp"
#include "OnlineProcessor.hpp"

#include "TH1.h"
#include "TFile.h"
#include "TCanvas.h"

TPad *OnlineProcessor::cd(const unsigned int &index_){
	if(!display_mode || index_ >= num_hists){ return NULL; }
	pad = (TPad*)(can->cd(index_+1));
	plot = *(plottable_hists.begin()+which_hists[index_]);
	return pad;
}

OnlineProcessor::OnlineProcessor(){
	display_mode = false;
}

OnlineProcessor::~OnlineProcessor(){
	if(display_mode){
		can->Close();
		delete can;
		delete[] which_hists;
	}
}

Plotter* OnlineProcessor::GetPlot(const unsigned int &index_){
	if(!display_mode || index_ >= num_hists){ return NULL; }
	return (plot = *(plottable_hists.begin()+which_hists[index_]));
}

/// Activate display of histograms to TCanvas.
bool OnlineProcessor::SetDisplayMode(const unsigned int &cols_/*=2*/, const unsigned int &rows_/*=2*/){
	if(display_mode){ return false; }

	num_hists = cols_*rows_;
	canvas_cols = cols_;
	canvas_rows = rows_;
	
	// Setup arrays.
	which_hists = new int[cols_*rows_];
	
	// Set the histogram ids for all TPads.
	for(unsigned int i = 0; i < num_hists; i++){
		which_hists[i] = -1;
	}
	
	// Setup the root canvas for plotting.
	can = new TCanvas("scanner_c1", "Scanner Canvas");
	
	return (display_mode=true);
}

bool OnlineProcessor::ChangeHist(const unsigned int &index_, const unsigned int &hist_id_){
	if(!display_mode || index_ >= num_hists || hist_id_ >= plottable_hists.size()){ return false; }
	which_hists[index_] = hist_id_;
	Refresh(index_);
	return true;
}

bool OnlineProcessor::ChangeHist(const unsigned int &index_, const std::string &hist_name_){
	if(!display_mode || index_ >= num_hists){ return false; }
	
	int count = 0;
	for(std::vector<Plotter*>::iterator iter = plottable_hists.begin(); iter != plottable_hists.end(); iter++){
		if(strcmp((*iter)->GetName().c_str(), hist_name_.c_str()) == 0){
			which_hists[index_] = count;
			Refresh(index_);
			return true;
		}
		count++;
	}
	
	return false;
}

bool OnlineProcessor::SetXrange(const unsigned int &index_, const double &xmin_, const double &xmax_){
	if(!GetPlot(index_)){ return false; }
	plot->SetXrange(xmin_, xmax_);
	Refresh(index_);
	return true;
}

bool OnlineProcessor::SetYrange(const unsigned int &index_, const double &ymin_, const double &ymax_){
	if(!GetPlot(index_)){ return false; }
	plot->SetYrange(ymin_, ymax_);
	Refresh(index_);
	return true;
}

bool OnlineProcessor::SetRange(const unsigned int &index_, const double &xmin_, const double &xmax_, const double &ymin_, const double &ymax_){
	if(!GetPlot(index_)){ return false; }
	plot->SetXrange(xmin_, xmax_);
	plot->SetYrange(ymin_, ymax_);
	Refresh(index_);
	return true;
}

bool OnlineProcessor::ResetXrange(const unsigned int &index_){
	if(!GetPlot(index_)){ return false; }
	plot->GetHist()->GetXaxis()->UnZoom();
	Refresh(index_);
	return true;
}

bool OnlineProcessor::ResetYrange(const unsigned int &index_){
	if(!GetPlot(index_)){ return false; }
	plot->GetHist()->GetYaxis()->UnZoom();
	Refresh(index_);
	return true;
}

bool OnlineProcessor::ResetRange(const unsigned int &index_){
	if(!GetPlot(index_)){ return false; }
	plot->GetHist()->GetXaxis()->UnZoom();
	plot->GetHist()->GetYaxis()->UnZoom();
	Refresh(index_);
	return true;
}

bool OnlineProcessor::ToggleLogX(const unsigned int &index_){
	if(!cd(index_) || plot->GetXmin() <= 0.0){ return false; }
	plot->ToggleLogX();
	Refresh(index_);
	return true;
}

bool OnlineProcessor::ToggleLogY(const unsigned int &index_){
	if(!cd(index_) || plot->GetYmin() <= 0.0){ return false; }
	plot->ToggleLogY();
	Refresh(index_);
	return true;
}

bool OnlineProcessor::ToggleLogZ(const unsigned int &index_){
	if(!cd(index_)){ return false; }
	plot->ToggleLogZ();
	Refresh(index_);
	return true;
}

/// Refresh a single online plot.
void OnlineProcessor::Refresh(const unsigned int &index_){
	if(cd(index_)){
		plot->Draw(pad);
		can->Update();
	}
}

/// Refresh all online plots.
void OnlineProcessor::Refresh(){
	if(!display_mode){ return; }

	can->Clear();

	// Divide the canvas into TPads.
	can->Divide(canvas_cols, canvas_rows);

	// Set the histogram ids for all TPads.
	for(unsigned int i = 0; i < num_hists; i++){
		if(which_hists[i] >= 0){
			cd(i);
			plot->Draw(pad);
		}
	}

	can->Update();
}

/** Zero a diagnostic histogram.
  *  param[in]  hist_id_ Histogram ID index.
  *  return True if the histogram exists and false otherwise.
  */
bool OnlineProcessor::Zero(const unsigned int &hist_id_){
	if(hist_id_ >= plottable_hists.size()){ return false; }
	plottable_hists.at(hist_id_)->Zero();
	return true;
}

/// Add a processor's histograms to the list of plottable items.
void OnlineProcessor::AddHists(Processor *proc){
	std::vector<Plotter*> processor_hists;
	proc->GetHists(processor_hists);
	for(std::vector<Plotter*>::iterator iter = processor_hists.begin(); iter != processor_hists.end(); iter++){
		plottable_hists.push_back((*iter));
	}
	processor_hists.clear();
}

/// Add a single histogram to the list of plottable items.
void OnlineProcessor::AddHist(Plotter *hist_){
	plottable_hists.push_back(hist_);
}

/// Write a histogram to a root TTree.
int OnlineProcessor::WriteHists(TFile *file_, const std::string &dirname_/*="hists"*/){
	if(!file_){ return -1; }

	file_->mkdir(dirname_.c_str());
	file_->cd(dirname_.c_str());

	int count = 0;
	for(std::vector<Plotter*>::iterator iter = plottable_hists.begin(); iter != plottable_hists.end(); iter++){
		(*iter)->Write();
		count++;
	}
	
	return count;
}

/// Display a list of available plots.
void OnlineProcessor::PrintHists(){
	std::cout << "OnlineProcessor: Displaying list of plottable histograms.\n";
	
	if(plottable_hists.empty()){
		std::cout << " NONE\n";
		return;
	}
	
	int count = 0;
	for(std::vector<Plotter*>::iterator iter = plottable_hists.begin(); iter != plottable_hists.end(); iter++){
		std::cout << " " << count++ << ": " << (*iter)->GetName() << "\t" << (*iter)->GetHist()->GetTitle() << std::endl;
	}
}
