#include "3d.h"
#include <math.h>
#include <windows.h>
#include <mmsystem.h>
#
#define SAMPLE_RATE (24000)
#define BUF_SZ (24000)
static HWAVEOUT wout;
static HANDLE sema;
static WAVEHDR hdr;
static int64_t freq=0,sample=0;
static double vol=0.1;
void SetVolume(double v) {
	vol=v;
}
double GetVolume() {
	return vol;
}
static int wCB(char *_out) {
	char *out = (char*)_out;
    unsigned int i;
    for( i=0; i<BUF_SZ; i++ ) {
		double t=(double)++sample/SAMPLE_RATE;
		double amp=-1.0+2.0*round(fmod(2.0*t*freq,1.0));
		int64_t maxed=(amp>0)?255:0;
		maxed*=vol;
		if(!freq) maxed=0;
		out[i]=maxed+0x7f;
		//out[2*i+1]=maxed;
	}
	return 0;
}
static __cb(HANDLE hwo,UINT msg,DWORD user,DWORD dw1,DWORD dw2) {
	if(msg==WOM_DONE)
		ReleaseSemaphore((HANDLE)user,1,NULL);
}
void InitSound() {
	WAVEFORMATEX format;
	format.wFormatTag=WAVE_FORMAT_PCM;
	format.nChannels=1;
	format.wBitsPerSample=8;
	format.nSamplesPerSec=SAMPLE_RATE;
	format.nBlockAlign=format.nChannels*format.wBitsPerSample/8;
	format.nAvgBytesPerSec=format.nSamplesPerSec*format.nBlockAlign;
	format.cbSize=0;
	sema=CreateSemaphore(NULL,1,1,NULL);
	assert(MMSYSERR_NOERROR==waveOutOpen(&wout,WAVE_MAPPER,&format,__cb,sema,CALLBACK_FUNCTION));
	hdr.lpData = TD_MALLOC(BUF_SZ*format.wBitsPerSample/8);
	hdr.dwBufferLength = BUF_SZ*format.wBitsPerSample/8;
	hdr.dwBytesRecorded=0;
	hdr.dwUser=0;
	hdr.dwFlags=0;
	hdr.dwLoops=0;
	hdr.lpNext=NULL;
	hdr.reserved=0;
}
void SndFreq(int64_t f) {
	freq=f;
	wCB(hdr.lpData);
	hdr.dwFlags|=WHDR_BEGINLOOP|WHDR_ENDLOOP;
	hdr.dwLoops=INFINITE;
	waveOutPrepareHeader(wout,&hdr,sizeof(hdr));
	waveOutWrite(wout,&hdr,sizeof(hdr));
}
