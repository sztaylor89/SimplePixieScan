#include <iostream>
#include <vector>

#include "TApplication.h"
#include "TSpectrum.h"
#include "TMarker.h"
#include "TLine.h"
#include "TCanvas.h"
#include "TTree.h"
#include "TF1.h"
#include "TH1.h"
#include "TH2.h"
#include "TGraph.h"

#include "TFile.h"
#include "TNamed.h"
#include "TDirectory.h"

#include "simpleTool.hpp"
#include "simpleGui.hpp"

// Write a TNamed to a root TFile.
template <typename T>
void writeTNamed(const std::string &name_, const T &val_){
	std::stringstream stream; stream << val_;
	TNamed named(name_.c_str(), stream.str().c_str());
	named.Write();
}

// Return the range of bins containing an upper and lower point, rounded to the nearest bins.
void findBins(TH1 *h, const double &xstart_, const double &xstop_, int &lowBin, int &highBin){
	if(xstart_ <= h->GetXaxis()->GetXmin()) lowBin = 1;
	else                                    lowBin = h->FindBin(xstart_);
	if(xstop_ >= h->GetXaxis()->GetXmax())  highBin = h->GetNbinsX();
	else                                    highBin = h->FindBin(xstop_);
}

// Return the number of counts in a 1-d histogram in the range [low, high], rounded to the nearest bins.
//  NOTE: This function returns the number of counts in a histogram, not the integral.
double summation(TH1 *h, const double &xstart_, const double &xstop_){
	int lowBin, highBin;
	findBins(h, xstart_, xstop_, lowBin, highBin);
	double retval = 0;
	for(int i = lowBin; i <= highBin; i++)
		retval += h->GetBinContent(i);
	return retval;
}

// Return the number of counts under a TF1 in the range [low, high], rounded to the nearest bins.
//  NOTE: This function returns the number of counts under a curve, not the function integral.
double summation(TH1 *h, TF1 *f, const double &xstart_, const double &xstop_){
	int lowBin, highBin;
	findBins(h, xstart_, xstop_, lowBin, highBin);
	double retval = 0;
	double xlo, xhi;
	for(int i = lowBin; i <= highBin; i++){
		xlo = h->GetBinLowEdge(i);
		xhi = xlo + h->GetBinWidth(i);
		retval += f->Integral(xlo, xhi)/(xhi-xlo);
	}
	return retval;
}

void calculateP2(double *x, double *y, double *p, bool log=false){
	double x1[3], x2[3];
	for(size_t i = 0; i < 3; i++){
		if(!log) x1[i] = x[i];
		else     x1[i] = std::log(x[i]);
		x2[i] = std::pow(x1[i], 2);
	}

	p[2] = (y[2]-y[0]+(y[0]-y[1])*(x1[2]-x1[0])/(x1[1]-x1[0]))/(x2[2]-x2[0]-(x2[1]-x2[0])*(x1[2]-x1[0])/(x1[1]-x1[0]));
	p[1] = (y[1]-y[0]-(x2[1]-x2[0])*p[2])/(x1[1]-x1[0]);
	p[0] = y[0] - p[1]*x1[0] - p[2]*x2[0];
}

class specFitter : public simpleHistoFitter {
  private:
	double Xmin, Xmax;
	double Xrange;
	int nBins;

	bool polyBG;
	bool gausFit;

	bool logXaxis;
	bool logYaxis;

	bool noPeakMode;
	bool strictMode;
	bool waitRun;
	bool skipNext;

	TDirectory *cdir;

	GuiWindow *win;

	void setupControlPanel();

	void updateAxes(TH1 *h1_);

	TGraph *convertHisToGraph(TH1 *h_, const double &xLow, const double &xHigh);

  public:
	specFitter() : simpleHistoFitter(), polyBG(false), gausFit(true), logXaxis(false), logYaxis(false), noPeakMode(false), strictMode(true), waitRun(true), skipNext(false) { }

	bool fitSpectrum(TH1 *h_, const int &binID_);

	void addChildOptions();
	
	bool processChildArgs();

	bool process();
};

void specFitter::setupControlPanel(){
	// Initialize root graphics classes.
	initRootGraphics();

	// Declare a new options menu.
	win = new GuiWindow(gClient->GetRoot(), "Background", 200, 400);

	// Add buttons to the menu..
	if(!polyBG){ // Order matters for radio buttons.
		win->AddRadio("expo");
		win->AddRadio("poly2", &polyBG);
	}
	else{
		win->AddRadio("poly2", &polyBG);
		win->AddRadio("expo");
	}

	win->NewGroup("Peak");
	if(gausFit){ // Order matters for radio buttons.
		win->AddRadio("gauss", &gausFit);
		win->AddRadio("landau");
	}
	else{
		win->AddRadio("landau");
		win->AddRadio("gauss", &gausFit);
	}

	win->NewGroup("Options");
	win->AddCheckbox("no-peak", &noPeakMode, noPeakMode);
	win->AddCheckbox("strict", &strictMode, strictMode);
	win->AddCheckbox("logx", &logXaxis, logXaxis);
	win->AddCheckbox("logy", &logYaxis, logYaxis);

	win->NewGroup("Control");
	win->AddButton("okay", &waitRun);
	win->AddButton("skip", &skipNext);
	win->AddQuitButton("exit");

	// Draw the menu.
	win->Update();
}

void specFitter::updateAxes(TH1 *h1_){
	if(logXaxis){ // Check the x-axis.
		if(can2->GetLogx() == 0){
			if(h1_->GetXaxis()->GetXmin() <= 0){
				std::cout << " Warning! Failed to set x-axis to log scale (xmin=" << h1_->GetXaxis()->GetXmin() << ").\n";
				logXaxis = false;
			}
			else{ 
				can2->SetLogx(); 
				can2->Update();
			}
		}
	}
	else if(can2->GetLogx() == 1){
		can2->SetLogx(0); 
		can2->Update();
	}

	if(logYaxis){ // Check the y-axis.
		if(can2->GetLogy() == 0){
			if(h1_->GetYaxis()->GetXmin() <= 0) h1_->SetMinimum(0.5);
			can2->SetLogy(); 
			can2->Update();
		}
	}
	else if(can2->GetLogy() == 1){ 
		can2->SetLogy(0); 
		can2->Update();
	}
}

void removeGraphPoints(TGraph *g_, const double &xLow, const double &xHigh){
	double x, y;
	int point = 0;
	while(point < g_->GetN()){
		g_->GetPoint(point, x, y);
		if(x < xLow){
			point++;
			continue;
		}
		else if(x > xHigh) break;
		g_->RemovePoint(point);
	}
}

TGraph *specFitter::convertHisToGraph(TH1 *h_, const double &xLow, const double &xHigh){
	std::vector<double> binCenters;
	std::vector<double> binWidths;
	std::vector<double> binContents;

	std::cout << "xLow=" << xLow << ", xHigh=" << xHigh << std::endl;

	double center;
	for(int i = 1; i <= h_->GetNbinsX(); i++){
		center = h_->GetBinCenter(i);
		if(center < xLow) continue;
		else if(center > xHigh) break;
		binCenters.push_back(center);
		binWidths.push_back(h_->GetBinWidth(i));
		binContents.push_back(h_->GetBinContent(i));
	}

	TGraph *output = new TGraph(binCenters.size());

	for(size_t i = 0; i < binCenters.size(); i++){
		output->SetPoint(i, binCenters.at(i), binContents.at(i));
		//output->SetPointError(i, binWidths.at(i)/2, std::sqrt(binContents.at(i)));
	}

	return output;
}


bool specFitter::fitSpectrum(TH1 *h_, const int &binID_){
	if(!h_) return false; 

	h_->Draw();
	can2->Update();

	// Wait for the user to change fit options.
	win->Wait(&waitRun);

	// Check if "quit" was selected.
	if(win->IsQuitting()) return false;

	// Check if the user wants to skip this spectrum.
	if(skipNext){
		skipNext = false;
		return false;
	}

	// Update log scales based on user input.
	updateAxes(h_);

	TMarker *marker;

	double xlo, ylo;
	double xhi, yhi;

	if(strictMode){	
		std::cout << " Mark bottom-left bounding point (xmin,ymin)\n";
		marker = (TMarker*)can2->WaitPrimitive("TMarker");
		xlo = marker->GetX();
		ylo = marker->GetY();
		delete marker;
	}	
	else{
		xlo = h_->GetXaxis()->GetXmin();
		ylo = 0;
	}

	std::cout << " Mark top-right bounding point (xmax,ymax)\n";
	marker = (TMarker*)can2->WaitPrimitive("TMarker");
	xhi = marker->GetX();
	yhi = marker->GetY();
	delete marker;

	// Convert the histogram to a TGraph.
	TGraph *graph;
	if(noPeakMode) graph = convertHisToGraph(h_, xlo, xhi);

	writeTNamed("winLow", xlo);
	writeTNamed("winHigh", xhi);

	if(!noPeakMode){
		h_->GetXaxis()->SetRangeUser(xlo, xhi);
		h_->GetYaxis()->SetRangeUser(ylo, yhi);
		h_->Draw();
	}
	else{
		graph = convertHisToGraph(h_, xlo, xhi);
		graph->GetXaxis()->SetRangeUser(xlo, xhi);
		graph->GetYaxis()->SetRangeUser(ylo, yhi);
		graph->SetMarkerStyle(2);
		graph->Draw("APL");
	}
	can2->Update();

	std::string xstr = "x";
	if(logXaxis) xstr = "log(x)";

	int numPeaks = 1;
	//std::cout << " How many peaks? "; std::cin >> numPeaks;
	
	if(numPeaks <= 0) 
		return false;

	double amplitude;
	double meanValue;
	
	double bgx[3];
	double bgy[3];
	double p[3];

	int nPars = 3;
	if(!noPeakMode) nPars += 3*numPeaks;

	std::stringstream stream1, stream2;

	if(!polyBG){ // Set initial function parameters.
		std::cout << "Mark exponential background (x0,y0)\n";
		marker = (TMarker*)can2->WaitPrimitive("TMarker");
		bgx[0] = marker->GetX();
		bgy[0] = marker->GetY();
		delete marker;
		std::cout << "Mark exponential background (x1,y1)\n";
		marker = (TMarker*)can2->WaitPrimitive("TMarker");	
		bgx[1] = marker->GetX();
		bgy[1] = marker->GetY();
		delete marker;
		std::cout << "Mark constant (xinf,yinf)\n";
		marker = (TMarker*)can2->WaitPrimitive("TMarker");
		bgx[2] = marker->GetX();
		bgy[2] = marker->GetY();
		delete marker;

		if(bgy[2] < bgy[1]) p[0] = bgy[2];
		else{
			if(debug) std::cout << " Warning! Y3 is less than Y2 (" << bgy[2] << " < " << bgy[1] << ")! Setting Y3=" << bgy[1]*0.9 << ".\n";
			p[0] = bgy[1]*0.9;
		}
		if(!logXaxis){
			p[2] = std::log((bgy[0]-p[0])/(bgy[1]-p[0]))/(bgx[0]-bgx[1]);
			p[1] = std::log(bgy[1]-p[0])-p[2]*bgx[1];
		}
		else{
			p[2] = std::log((bgy[0]-p[0])/(bgy[1]-p[0]))/std::log(bgx[0]/bgx[1]);
			p[1] = std::log(bgy[1]-p[0])-p[2]*std::log(bgx[1]);
		}
		
		stream1 << "[0]+exp([1]+[2]*" << xstr << ")";
	}
	else{ // Set initial function parameters.
		std::cout << "Mark linear background (x0,y0)\n";
		marker = (TMarker*)can2->WaitPrimitive("TMarker");
		bgx[0] = marker->GetX();
		bgy[0] = marker->GetY();
		delete marker;
		std::cout << "Mark linear background (x1,y1)\n";
		marker = (TMarker*)can2->WaitPrimitive("TMarker");	
		bgx[1] = marker->GetX();
		bgy[1] = marker->GetY();
		delete marker;
		std::cout << "Mark linear background (x2,y2)\n";
		marker = (TMarker*)can2->WaitPrimitive("TMarker");	
		bgx[2] = marker->GetX();
		bgy[2] = marker->GetY();
		delete marker;

		calculateP2(bgx, bgy, p, logXaxis);
		
		stream1 << "[0]+" << xstr << "*[1]+" << xstr << "^2*[2]";
	}

	if(debug) std::cout << " p0 = " << p[0] << ", p1 = " << p[1] << ", p2 = " << p[2] << std::endl;

	if(!noPeakMode){ // Add the peak function to the function string.
		for(int i = 0; i < numPeaks; i++){
			if(gausFit) stream2 << "+[" << 3*i+3 << "]*TMath::Gaus(" << xstr << ", [" << 3*i+4 << "], [" << 3*i+5 << "])";
			else        stream2 << "+[" << 3*i+3 << "]*TMath::Landau(" << xstr << ", [" << 3*i+4 << "], [" << 3*i+5 << "])";
		}
	}

	std::string totalString = stream1.str() + stream2.str();

	// Define the total fit function.
	if(debug) std::cout << " Declaring function \"" << totalString << "\".\n";
	TF1 *func = new TF1("func", totalString.c_str(), xlo, xhi);

	func->SetParameter(0, p[0]);
	func->SetParameter(1, p[1]);
	func->SetParameter(2, p[2]);

	if(!noPeakMode){ // Get the initial conditions of the peak.
		double bkgA, fwhmLeft, fwhmRight;
		for(int i = 0; i < numPeaks; i++){
			std::cout << "Mark peak " << i+1 << " maximum\n";
			marker = (TMarker*)can2->WaitPrimitive("TMarker");
			amplitude = marker->GetY();
			meanValue = marker->GetX();
			if(!polyBG){
				if(!logXaxis) bkgA = p[0]+std::exp(p[1]+p[2]*meanValue);
				else      bkgA = p[0]+std::exp(p[1])*std::pow(meanValue, p[2]);
			}
			else{
				if(!logXaxis) bkgA = p[0]+p[1]*meanValue+p[2]*meanValue*meanValue;
				else      bkgA = p[0]+p[1]*std::log(meanValue)+p[2]*std::pow(std::log(meanValue), 2);
			}
			amplitude = amplitude-bkgA;
			delete marker;
			TLine line(xlo, amplitude*0.5+bkgA, xhi, amplitude*0.5+bkgA);
			line.Draw("SAME");
			can2->Update();
			std::cout << "Mark left and right of peak " << i+1 << "\n";
			marker = (TMarker*)can2->WaitPrimitive("TMarker");
			fwhmLeft = marker->GetX();
			delete marker;
			marker = (TMarker*)can2->WaitPrimitive("TMarker");
			fwhmRight = marker->GetX();
			delete marker;
			if(gausFit) func->SetParameter(3*i+3, amplitude);
			else        func->SetParameter(3*i+3, amplitude*5.9);
			if(!logXaxis){
				func->SetParameter(3*i+4, meanValue);
				func->SetParameter(3*i+5, (fwhmRight-fwhmLeft)/2.35);
			}
			else{
				func->SetParameter(3*i+4, std::log(meanValue));
				double sig1 = std::log(fwhmRight)-meanValue; // Right side of the maximum.
				double sig2 = meanValue-std::log(fwhmLeft); // Left side of the maximum.
				func->SetParameter(3*i+5, (sig1+sig2)/2);
			}
		}
	}

	// Define the fitting limits
	double fitlow, fithigh;
	if(!noPeakMode){
		if(strictMode){
			func->Draw("SAME");
			can2->Update();
			std::cout << "Mark upper and lower limits of fitting region\n";
			marker = (TMarker*)can2->WaitPrimitive("TMarker");
			fitlow = marker->GetX();
			delete marker;
			marker = (TMarker*)can2->WaitPrimitive("TMarker");
			fithigh = marker->GetX();
			delete marker;
		}
		else{
			fitlow = bgx[0];
			fithigh = bgx[2];
		}
	}
	else{
		double removeLow, removeHigh;
		func->Draw("SAME");
		can2->Update();

		std::cout << "Mark upper and lower limits of fitting region to left of peak\n";
		marker = (TMarker*)can2->WaitPrimitive("TMarker");
		fitlow = marker->GetX();
		delete marker;
		marker = (TMarker*)can2->WaitPrimitive("TMarker");
		removeLow = marker->GetX();
		delete marker;

		std::cout << "Mark upper and lower limits of fitting region to right of peak\n";
		marker = (TMarker*)can2->WaitPrimitive("TMarker");
		removeHigh = marker->GetX();
		delete marker;
		marker = (TMarker*)can2->WaitPrimitive("TMarker");
		fithigh = marker->GetX();
		delete marker;

		removeGraphPoints(graph, removeLow, removeHigh);
	}

	writeTNamed("fitLow", fitlow);
	writeTNamed("fitHigh", fithigh);

	func->SetRange(fitlow, fithigh);

	if(debug){ // Print the initial conditions.
		std::cout << " Initial:\n";
		for(int i = 0; i < nPars; i++){
			std::cout << "  p[" << i << "] = " << func->GetParameter(i) << std::endl;
		}
	}
	
	// Fit the spectrum.
	if(!noPeakMode)
		h_->Fit(func, "QN0R");
	else
		graph->Fit(func, "QN0R");

	if(debug){ // Print the fit results.
		std::cout << " Final:\n";
		for(int i = 0; i < nPars; i++){
			std::cout << "  p[" << i << "] = " << func->GetParameter(i) << std::endl;
		}
	}

	// Report the chi^2 of the fit.
	std::cout << " Reduced chi^2 = " << func->GetChisquare()/func->GetNDF() << std::endl;
	writeTNamed("chi2", func->GetChisquare());
	writeTNamed("NDF", func->GetNDF());

	// Draw the total fit.
	func->Draw("SAME");

	TF1 *bkgfunc = NULL;
	bkgfunc = new TF1("bkgfunc", stream1.str().c_str(), xlo, xhi);
	
	bkgfunc->SetLineColor(kMagenta+1);
	bkgfunc->SetParameter(0, func->GetParameter(0));
	bkgfunc->SetParameter(1, func->GetParameter(1));
	bkgfunc->SetParameter(2, func->GetParameter(2));
	bkgfunc->Draw("SAME");

	// Draw the composite gaussians.
	double totalIntegral = 0;
	double histSum = summation(h_, xlo, xhi);
	double integral = summation(h_, bkgfunc, xlo, xhi);
	totalIntegral += integral;
	if(debug){
		std::cout << " Integrated Counts:\n";
		std::cout << "  background: " << integral << "\n";
	}
	writeTNamed("counts", histSum);
	writeTNamed("Ibkg", integral);
	TF1 *lilfunc = NULL;
	if(!noPeakMode){
		//TF1 **lilfuncs = new TF1*[numPeaks+1];
		//for(int i = 0; i < numPeaks; i++){
			lilfunc = new TF1("peakfunc", stream2.str().c_str(), xlo, xhi);
			lilfunc->SetLineColor(kGreen+3);
			lilfunc->SetParameter(0, func->GetParameter(3));
			lilfunc->SetParameter(1, func->GetParameter(4));
			lilfunc->SetParameter(2, func->GetParameter(5));
			lilfunc->Draw("SAME");

			integral = summation(h_, lilfunc, xlo, xhi);
			totalIntegral += integral;
		//}
	}
	else{ integral = 0; }
	can2->Update();

	// Wait for the user to press "okay" on the control panel.
	std::cout << " Press \"okay\" to continue...\n";
	win->Wait(&waitRun);
	std::cout << std::endl;

	if(debug){
		std::cout << "  peak: " << integral << "\n";
		std::cout << "  total: " << totalIntegral << "\n";
		std::cout << "  hist: " << histSum << "\n";
	}

	writeTNamed("Ipeak", integral);
	writeTNamed("Itotal", totalIntegral);

	if(noPeakMode) graph->Write("graph");
	h_->Write();
	func->Write();
	bkgfunc->Write();
	if(!noPeakMode) lilfunc->Write();
	
	cdir->mkdir("pars")->cd();
	const std::string parnames[6] = {"p0", "p1", "p2", "p3", "p4", "p5"};
	for(int i = 0; i < nPars; i++) writeTNamed(parnames[i], func->GetParameter(i));
	for(int i = 0; i < nPars; i++) writeTNamed(parnames[i]+"err", func->GetParError(i));
	
	delete func;
	delete bkgfunc;
	if(!noPeakMode) 
		delete lilfunc;
	else 
		delete graph;

	return true;
}

void specFitter::addChildOptions(){
	addOption(optionExt("poly", no_argument, NULL, 'p', "", "Fit background spectrum with 2nd order polynomial."), userOpts, optstr);
	addOption(optionExt("landau", no_argument, NULL, 'l', "", "Fit peaks with landau distributions."), userOpts, optstr);
	addOption(optionExt("logx", no_argument, NULL, 0x0, "", "Log the x-axis of the projection histogram."), userOpts, optstr);
	addOption(optionExt("logy", no_argument, NULL, 0x0, "", "Log the y-axis of the projection histogram."), userOpts, optstr);
	addOption(optionExt("quick", no_argument, NULL, 0x0, "", "Prompts the user for less input."), userOpts, optstr); 
}

bool specFitter::processChildArgs(){
	if(userOpts.at(firstChildOption).active)
		polyBG = true;
	if(userOpts.at(firstChildOption+1).active)
		gausFit = false;
	if(userOpts.at(firstChildOption+2).active)
		logXaxis = true;
	if(userOpts.at(firstChildOption+3).active)
		logYaxis = true;
	if(userOpts.at(firstChildOption+4).active)
		strictMode = false;

	return true;
}

bool specFitter::process(){
	if(!h2d || !can2) return false;

	// Initialize the gui panel.
	setupControlPanel();

	TFile *ofile;
	if(!output_filename.empty())
		ofile = new TFile(output_filename.c_str(), "RECREATE");
	else
		ofile = new TFile("specFitter.root", "RECREATE");

	ofile->cd();
	writeTNamed("input", full_input_filename);
	if(!polyBG) writeTNamed("bkg", "expo");
	else        writeTNamed("bkg", "poly2");
	if(gausFit) writeTNamed("func", "gauss");
	else        writeTNamed("func", "landau");
	if(!logXaxis) writeTNamed("logx", "false");
	else          writeTNamed("logx", "true");
	if(!logYaxis) writeTNamed("logy", "false");
	else          writeTNamed("logy", "true");

	// Write the input histogram to file.
	h2d->Write("h2d");

	TH1D *h1 = getProjectionHist(h2d);
	h1->SetStats(0);

	nBins = h1->GetNbinsX();
	Xmin = h1->GetXaxis()->GetXmin();
	Xmax = h1->GetXaxis()->GetXmax();
	Xrange = Xmax-Xmin;

	can2->cd();

	// Update for log scales.
	updateAxes(h1);
	
	double binLow, binWidth;
	int numProjections = getNumProjections(h2d);
	for(int i = 1; i <= numProjections; i++){
		std::cout << " Processing channel ID " << i << "... ";
		if(getProjection(h1, h2d, i)){ 
			std::cout << "DONE\n";

			std::stringstream stream1;
			std::stringstream stream2; 
			stream1 << "bin";
			if(i < 10)       stream1 << "00" << i;
			else if(i < 100) stream1 << "0" << i;
			else             stream1 << i;
			
			binLow = getBinLowEdge(h2d, i);
			binWidth = getBinWidth(h2d, i);

			stream2 << stream1.str() << " [" << binLow << ", " << binLow+binWidth << "]";
			h1->SetTitle(stream2.str().c_str());

			h1->GetXaxis()->SetRangeUser(Xmin, Xmax);
			h1->GetYaxis()->SetRangeUser(0, 1.1*h1->GetBinContent(h1->GetMaximumBin()));

			cdir = ofile->mkdir(stream1.str().c_str());
			cdir->cd();

			writeTNamed("binLow", binLow);
			writeTNamed("binWidth", binWidth);

			// Handle log scale limit errors.
			if(logYaxis) h1->SetMinimum(0.5);

			// Fit the spectrum.
			fitSpectrum(h1, i);

			// Check if "quit" was selected.
			if(win->IsQuitting()){
				std::cout << "  EXITING!\n";	
				break;
			}
		}
		else std::cout << "FAILED\n";
	}
	
	delete h1;

	ofile->Close();
	
	return true;
}

int main(int argc, char *argv[]){
	specFitter obj;
	
	return obj.execute(argc, argv);
}
