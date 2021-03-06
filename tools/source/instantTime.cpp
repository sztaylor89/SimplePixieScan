#include <iostream>
#include <string>

#include "TFile.h"
#include "TTree.h"
#include "TBranch.h"

#include "simpleTool.hpp"
#include "Structures.h"

class instantTime : public simpleTool {
  public:
	instantTime() : simpleTool() { }
	
	void addOptions();
	
	bool processArgs();
	
	int execute(int argc, char *argv[]);
};

void instantTime::addOptions(){
}

bool instantTime::processArgs(){
	return true;
}

int instantTime::execute(int argc, char *argv[]){
	if(!setup(argc, argv))
		return 0;	

	if(input_filename.empty()){
		std::cout << " Error: Must specify input filename!\n";
		return 1;
	}

	std::cout << " Processing " << input_filename << ".\n";
	if(!openInputFile()) return false;

	if(!loadInputTree()){
		std::cout << "  Failed to load input tree!\n";
		return 2;
	}

	TBranch *branch = NULL;
	LogicStructure *ptr = NULL;

	intree->SetBranchAddress("logic", &ptr, &branch);

	if(!branch){
		std::cout << " Error: Failed to load branch \"trigger\" from input TTree.\n";
		return false;
	}

	if(output_filename.empty()){
		std::cout << " Warning: Output filename not specified. Setting output filename to \"instantaneous.root\".\n";
		output_filename = "instantaneous.root";
	}

	if(!openOutputFile()){
		std::cout << " Error: Failed to load output file \"" << output_filename << "\".\n";
		return 3;
	}

	unsigned int count = 0;
	double firstTime = 0;	
	double prevTime = 0;
	double currTime = 0;
	
	double tdiff;
	
	outtree = new TTree("t", "tree");
	outtree->Branch("tdiff", &tdiff);
	outtree->Branch("time", &currTime);

	progressBar pbar;
	pbar.start(intree->GetEntries());

	for(int i = 0; i < intree->GetEntries(); i++){
		pbar.check(i);
		intree->GetEntry(i);
		if(ptr->mult == 0)
			continue;
		for(unsigned int j = 0; j < ptr->mult; j++){
			if(count++ != 0){
				currTime = ptr->time.at(j)-firstTime;
				tdiff = currTime-prevTime;
				outtree->Fill();
				prevTime = currTime;
			}
			else{ firstTime = ptr->time.at(j); }
		}
	}

	pbar.finalize();

	std::cout << " First event time   = " << firstTime*8E-9 << " s.\n";
	std::cout << " Total elapsed time = " << currTime*8E-9 << " s.\n";

	outfile->cd();
	outtree->Write();
	
	return 0;
}

int main(int argc, char *argv[]){
	instantTime obj;
	
	return obj.execute(argc, argv);
}
