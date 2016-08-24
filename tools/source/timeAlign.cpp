#include <iostream>
#include <sstream>
#include <fstream>

#include "TApplication.h"
#include "TCanvas.h"
#include "TFile.h"
#include "TTree.h"
#include "TH1.h"
#include "TH2.h"
#include "TF1.h"
#include "TH1D.h"
#include "TMarker.h"

#include "main.hpp"

bool Process(TH2 *h_, TCanvas *can_){
	if(!h_ || !can_) return false;
	
	can_->cd()->SetLogy();
	
	TH1D *h1 = new TH1D("h1", "", h_->GetXaxis()->GetNbins(), h_->GetXaxis()->GetXmin(), h_->GetXaxis()->GetXmax());
	TF1 *f1 = new TF1("f1", "gaus", 0, 1);
	TFitResultPtr fitResult;
	TMarker *m1;

	double xmin, xmax;

	std::ofstream ofile("fitresults.dat");
	ofile << "id\tA\tmean\tsigma\tchi2\n";

	for(int i = 1; i <= h_->GetYaxis()->GetNbins(); i++){
		std::cout << " Processing channel ID " << i << "... ";
		if(GetProjectionX(h1, h_, i)){ 
			std::cout << "DONE\n";

			std::stringstream stream; stream << h_->GetYaxis()->GetBinLowEdge(i);
			h1->SetTitle(stream.str().c_str());
			
			can_->Clear();
			h1->Draw();
			can_->Update();
			
			m1 = (TMarker*)can_->WaitPrimitive("TMarker");
			xmin = m1->GetX();
			m1->Delete();
			m1 = (TMarker*)can_->WaitPrimitive("TMarker");
			xmax = m1->GetX();
			m1->Delete();
			
			std::cout << "  Range: " << xmin << ", " << xmax << std::endl;
			
			f1->SetRange(xmin, xmax);
			h1->Fit(f1, "QR");
			f1->Draw("SAME");
			
			// Output the fit results.
			std::cout << "  Fit: chi^2 = " << f1->GetChisquare()/f1->GetNDF() << ", t0 = " << f1->GetParameter(1) << " ns\n";
			ofile << stream.str() << "\t" << f1->GetParameter(0) << "\t" << f1->GetParameter(1) << "\t" << f1->GetParameter(2) << "\t" << f1->GetChisquare()/f1->GetNDF() << std::endl;
			
			can_->WaitPrimitive();
		}
		else std::cout << "FAILED\n";
	}
	
	delete h1;
	delete f1;
	
	return true;
}

int main(int argc, char *argv[]){
	return Execute(argc, argv);
}
