#include <cstdio>
#include "wave.h"
extern "C" {
#include "sonic.h"

}
#define BUFFER_SIZE 2048
int main3(int argc, char* argv[]) {
	waveFile inFile, outFile = NULL;
	sonicStream stream;
	char file_name[] = "output_wav.wav";
	char out_file_name[] = "output_wav_2.wav";
	float speed = 0.5;
	//FILE* pFile = NULL;
	//FILE* pOutFile = NULL;
	//pFile = fopen(file_name, "rb");
	short inBuffer[BUFFER_SIZE], outBuffer[BUFFER_SIZE];
	int sampleRate = 44100;
	int channel_num = 2;
	inFile = openInputWaveFile(file_name, &sampleRate, &channel_num);
	outFile = openOutputWaveFile(out_file_name, sampleRate, channel_num);

	stream = sonicCreateStream(sampleRate, channel_num);
	sonicSetSpeed(stream, speed);

	int samplesRead, samplesWritten;
	do {
		samplesRead = readFromWaveFile(inFile, inBuffer, BUFFER_SIZE / channel_num);
		if (samplesRead == 0) {
			sonicFlushStream(stream);
		}
		else {
			sonicWriteShortToStream(stream, inBuffer, samplesRead);
		}
		do {
			samplesWritten = sonicReadShortFromStream(stream, outBuffer,
				BUFFER_SIZE / channel_num);
			//printf("%d\n", samplesWritten);
			if (samplesWritten > 0) {
				writeToWaveFile(outFile, outBuffer, samplesWritten);
			}
		} while (samplesWritten > 0);
	} while (samplesRead > 0);

	sonicDestroyStream(stream);
	closeWaveFile(inFile);
	closeWaveFile(outFile);
	return 0;
}