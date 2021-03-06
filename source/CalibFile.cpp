#include <iostream>
#include <sstream>
#include <fstream>
#include <cmath>

#include "TFile.h"
#include "TObjString.h"

#include "CalibFile.hpp"

#include "XiaData.hpp"
#include "ScanInterface.hpp"
#include "CTerminal.h"

const double deg2rad = 0.0174532925;
const double rad2deg = 57.295779579;

CalibEntry dummyCalib(NULL, NULL, NULL);

PositionCal::PositionCal(const std::vector<std::string> &pars_) : CalType(0), r0(0.0), theta(0.0), phi(0.0) { 
	defaultVals = false;
	int index = 0;
	for(std::vector<std::string>::const_iterator iter = pars_.begin(); iter != pars_.end(); iter++){
		if(index == 0) id = strtol(iter->c_str(), NULL, 0);
		else if(index == 1) r0 = strtod(iter->c_str(), NULL);
		else if(index == 2) theta = strtod(iter->c_str(), NULL)*deg2rad;
		else if(index == 3) phi = strtod(iter->c_str(), NULL)*deg2rad;
		index++;
	}
}

std::string PositionCal::Print(bool fancy/*=true*/){
	std::stringstream output;
	if(fancy) output << " id=" << id << ", r0=" << r0 << ", theta=" << theta << ", phi=" << phi;
	else output << id << "\t" << r0 << "\t" << theta;
	return output.str();
}

TimeCal::TimeCal(const std::vector<std::string> &pars_) : CalType(0), t0(0.0) { 
	defaultVals = false;
	int index = 0;
	for(std::vector<std::string>::const_iterator iter = pars_.begin(); iter != pars_.end(); iter++){
		if(index == 0) id = strtol(iter->c_str(), NULL, 0);
		else if(index == 1) t0 = strtod(iter->c_str(), NULL);
		index++;
	}
}

bool TimeCal::GetCalTime(double &T){
	T = T - t0;
	return true;
}

double TimeCal::GetCalTime(const double &time_){
	return (time_ - t0);
}

std::string TimeCal::Print(bool fancy/*=true*/){
	std::stringstream output;
	if(fancy) output << " id=" << id << ", t0=" << t0;
	else output << id << "\t" << t0;
	return output.str();
}

EnergyCal::EnergyCal(const std::vector<std::string> &pars_) : CalType(0) { 
	defaultVals = false;
	int index = 0;
	for(std::vector<std::string>::const_iterator iter = pars_.begin(); iter != pars_.end(); iter++){
		if(index == 0) id = strtol(iter->c_str(), NULL, 0);
		else vals.push_back(strtod(iter->c_str(), NULL));
		index++;
	}
}

bool EnergyCal::GetCalEnergy(const double &adc_, double &E){
	if(vals.empty()) return false;
	E = 0.0;
	for(size_t i = 0; i < vals.size(); i++)
		E += vals.at(i)*std::pow(adc_, i);
	return true;
}

double EnergyCal::GetCalEnergy(const double &adc_){
	if(vals.empty()) return adc_;
	double output = 0.0;
	for(size_t i = 0; i < vals.size(); i++)
		output += vals.at(i)*std::pow(adc_, i);
	return output;
}

std::string EnergyCal::Print(bool fancy/*=true*/){
	std::stringstream output;
	int index = 0;
	if(fancy) output << " id=" << id;
	else output << id;
	if(!vals.empty()){
		for(std::vector<double>::iterator iter = vals.begin(); iter != vals.end(); iter++){
			if(fancy) output << ", p" << index++ << "=" << (*iter);
			else output << "\t" << (*iter);
		}
	}
	else if(fancy){ output << ", EMPTY"; }
	return output.str();
}

bool CalibFile::_load(const char *filename_, const int &type_){
	std::ifstream calibfile(filename_);
	if(!calibfile.good()){
		if(type_==0)
			std::cout << "calibfile: \033[1;31mERROR! Failed to open time calibration file '" << filename_ << "'!\033[0m\n";
		else if(type_==1)
			std::cout << "calibfile: \033[1;31mERROR! Failed to open energy calibration file '" << filename_ << "'!\033[0m\n";
		else if(type_==2)
			std::cout << "calibfile: \033[1;31mERROR! Failed to open position calibration file '" << filename_ << "'!\033[0m\n";
		return false;
	}

	int line_num = 1;
	int prevID = -1;
	int readID;
	std::vector<std::string> values;
	std::string line;
	while(true){
		getline(calibfile, line);
		
		if(calibfile.eof() || !calibfile.good())
			break;
		else if(line.empty() || line[0] == '#')
			continue;
		
		split_str(line, values, '\t');
		
		if(values.empty())
			continue;
		
		readID = (int)strtol(values[0].c_str(), NULL, 0);
		
		if(readID < 0 || readID <= prevID){
			if(type_==0)
				std::cout << "CalibFile: \033[1;33mWARNING! On line " << line_num++ << " of time calibration file, invalid id number (" << readID << "). Ignoring.\033[0m\n";
			else
				std::cout << "CalibFile: \033[1;33mWARNING! On line " << line_num++ << " of energy calibration file, invalid id number (" << readID << "). Ignoring.\033[0m\n";
			continue;
		}
		else if(readID > prevID+1){
			for(int i = prevID+1; i < readID; i++)
				if(type_==0)
					time_calib.push_back(TimeCal(i, 0.0));
				else if(type_==1)
					energy_calib.push_back(EnergyCal(i));
				else if(type_==2)
					position_calib.push_back(PositionCal(i, 0.5, 0.0, 0.0));
		}
		
		prevID = readID;
		line_num++;
		if(type_==0)
			time_calib.push_back(TimeCal(values));
		else if(type_==1)
			energy_calib.push_back(EnergyCal(values));	
		else if(type_==2)
			position_calib.push_back(PositionCal(values));
	}
	
	return true;
}

CalibFile::CalibFile(const char *timeFilename_, const char *energyFilename_, const char *positionFilename_){ 
	Load(timeFilename_, energyFilename_, positionFilename_);
}

bool CalibFile::LoadTimeCal(const char *filename_){
	return _load(filename_, 0);
}

bool CalibFile::LoadEnergyCal(const char *filename_){
	return _load(filename_, 1);
}

bool CalibFile::LoadPositionCal(const char *filename_){
	return _load(filename_, 2);
}

bool CalibFile::Load(const char *timeFilename_, const char *energyFilename_, const char *positionFilename_){
	return (LoadTimeCal(timeFilename_) && LoadEnergyCal(energyFilename_) && LoadPositionCal(positionFilename_));
}

TimeCal *CalibFile::GetTimeCal(const unsigned int &id_){
	if(id_ >= time_calib.size()){ return NULL; }
	return &time_calib.at(id_);
}

TimeCal *CalibFile::GetTimeCal(XiaData *event_){
	return GetTimeCal(16*event_->modNum+event_->chanNum); 
}

EnergyCal *CalibFile::GetEnergyCal(const unsigned int &id_){
	if(id_ >= energy_calib.size()){ return NULL; }
	return &energy_calib.at(id_);
}

EnergyCal *CalibFile::GetEnergyCal(XiaData *event_){
	return GetEnergyCal(16*event_->modNum+event_->chanNum); 
}

PositionCal *CalibFile::GetPositionCal(const unsigned int &id_){
	if(id_ >= position_calib.size()){ return NULL; }
	return &position_calib.at(id_);
}

PositionCal *CalibFile::GetPositionCal(XiaData *event_){
	return GetPositionCal(16*event_->modNum+event_->chanNum); 
}

CalibEntry *CalibFile::GetCalibEntry(const unsigned int &id_){
	TimeCal *tcal = this->GetTimeCal(id_);
	EnergyCal *ecal = this->GetEnergyCal(id_);
	PositionCal *pcal = this->GetPositionCal(id_);
	return (new CalibEntry(tcal, ecal, pcal));
}

CalibEntry *CalibFile::GetCalibEntry(XiaData *event_){
	return GetCalibEntry(16*event_->modNum+event_->chanNum); 
}

void CalibFile::Debug(int mode){
	if(mode==0 || mode==3){
		std::cout << "Timing calibration:\n";
		for(size_t i = 0; i < time_calib.size(); i++)
			std::cout << time_calib[i].Print() << std::endl;
	}
	if(mode==1 || mode==3){
		std::cout << "Energy calibration:\n";
		for(size_t i = 0; i < energy_calib.size(); i++)
			std::cout << energy_calib[i].Print() << std::endl;
	}
	if(mode==2 || mode==3){
		std::cout << "Position calibration:\n";
		for(size_t i = 0; i < position_calib.size(); i++)
			std::cout << position_calib[i].Print() << std::endl;
	}
}

bool CalibFile::Write(TFile *f_){
	if(!f_ || !f_->IsOpen())
		return false;

	// Make the calibration directory.
	f_->mkdir("calib");

	// Make the time calibration directory.
	f_->mkdir("calib/time");
	f_->cd("calib/time");

	for(std::vector<TimeCal>::iterator iter = time_calib.begin(); iter != time_calib.end(); ++iter){
		if(iter->defaultVals) continue; // Skip entries with default values.
		TObjString str(iter->Print(false).c_str());
		str.Write();
	}

	// Make the energy calibration directory.
	f_->mkdir("calib/energy");
	f_->cd("calib/energy");

	for(std::vector<EnergyCal>::iterator iter = energy_calib.begin(); iter != energy_calib.end(); ++iter){
		if(iter->defaultVals) continue; // Skip entries with default values.
		TObjString str(iter->Print(false).c_str());
		str.Write();
	}
	
	// Make the position calibration directory.
	f_->mkdir("calib/position");
	f_->cd("calib/position");

	for(std::vector<PositionCal>::iterator iter = position_calib.begin(); iter != position_calib.end(); ++iter){
		if(iter->defaultVals) continue; // Skip entries with default values.
		TObjString str(iter->Print(false).c_str());
		str.Write();
	}
	
	// Return to the top level directory.
	f_->cd();

	return true;
}
