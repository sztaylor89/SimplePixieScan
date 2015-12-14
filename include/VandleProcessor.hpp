#ifndef VANDLE_PROCESSOR_HPP
#define VANDLE_PROCESSOR_HPP

#include "Processor.hpp"

#include "Structures.h"

class MapFile;

class VandleProcessor : public Processor{
  private:
	VandleStructure structure;
	VandleWaveform waveform;  

	Plotter *loc_L_tdiff_2d;
	Plotter *loc_R_tdiff_2d;
	Plotter *loc_L_energy_2d;
	Plotter *loc_R_energy_2d;
	Plotter *loc_L_phase_2d;
	Plotter *loc_R_phase_2d;
	Plotter *loc_1d;

	virtual bool HandleEvents();
	
  public:
	VandleProcessor(MapFile *map_);

	~VandleProcessor();
	
	virtual void GetHists(std::vector<Plotter*> &plots_);
};

#endif
