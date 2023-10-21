#include "3d.h"
#include <math.h>
#include <portaudio.h>
#define SAMPLE_RATE (24000)
static PaStream *pa_stream;
static int64_t freq=0,sample=0;
static double vol=0.1;
void SetVolume(double v) {
	vol=v;
}
double GetVolume() {
	return vol;
}
static int paCallback(const void *inp,void *_out,unsigned long fpb,const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData ) {
	char *out = (char*)_out;
    unsigned int i;
    for( i=0; i<fpb; i++ ) {
		double t=(double)++sample/SAMPLE_RATE;
		double amp=-1.0+2.0*round(fmod(2.0*t*freq,1.0));
		char maxed=(amp>0)?127:-127;
		maxed*=vol;
		if(!freq) maxed=0;
		out[2*i]=maxed;
		out[2*i+1]=maxed;
	}
	return 0;
}
void InitSound() {
	int err=Pa_Initialize();
	err=Pa_OpenDefaultStream(&pa_stream,0,2,paInt8,SAMPLE_RATE,256,&paCallback,&freq);
	Pa_StartStream(pa_stream);
}
void SndFreq(int64_t f) {
	freq=f;
	/*
	if(freq)
		Pa_StartStream(pa_stream);
	else
		Pa_StopStream(pa_stream);
	*/
}
