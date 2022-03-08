#include <cstdio>
#include "wave.h"
extern "C" {
#include "sonic.h"
#include <vcruntime_string.h>

}
#define BUFFER_SIZE 4096

void readSamples(short* bytes, short* buff, int maxSamples, int channel_num) {
	int bytePos = 0;
	short sample;
	for (int i = 0; i < maxSamples * channel_num; i++) {
		sample = bytes[bytePos++];
		//sample += (unsigned int)bytes[bytePos++] << 8;
		*buff++ = sample;
		//printf("%d\n", sample);
	}
}

void writeSamples(short* bytes, short* buff, int maxSamples, int channel_num) {
	int bytePos = 0;
	short sample;
	for (int i = 0; i < maxSamples * channel_num; i++) {
		sample = buff[i];
		bytes[bytePos++] = sample;
		//bytes[bytePos++] = sample >> 8;
		//printf("%d\n", sample);
	}
}

/**
* wave的做法。readSamples那样，强转为short指针，直接读取short，应该更快。readSamples2多了几步赋值、自增、位运算。
*/
void readSamples2(unsigned char* bytes, short* buff, int maxSamples, int channel_num) {
	int bytePos = 0;
	short sample;
	for (int i = 0; i < maxSamples * channel_num; i++) {
		sample = bytes[bytePos++];
		sample += (unsigned int)bytes[bytePos++] << 8;
		*buff++ = sample;
		//printf("%d\n", sample);
	}
}

void writeSamples2(unsigned char* bytes, short* buff, int maxSamples, int channel_num) {
	int bytePos = 0;
	short sample;
	for (int i = 0; i < maxSamples * channel_num; i++) {
		sample = buff[i];
		bytes[bytePos++] = sample;
		bytes[bytePos++] = sample >> 8;
		//printf("%d\n", sample);
	}
}


int main2(int argc, char* argv[]) {

	waveFile inFile, outFile = NULL;
	sonicStream stream;
	char file_name[] = "output.pcm";
	char out_file_name[] = "output_2.pcm";
	float speed = 2;

	FILE* pFile = NULL;
	FILE* pOutFile = NULL;
	pFile = fopen(file_name, "rb");
	pOutFile = fopen(out_file_name, "wb");

	unsigned char inBuf[BUFFER_SIZE] = { 0 }, outBuf[BUFFER_SIZE] = { 0 };
	short sampleInBuffer[BUFFER_SIZE / 2] = { 0 }, sampleOutBuffer[BUFFER_SIZE / 2] = { 0 };
	int sampleRate = 44100;
	int channel_num = 2;

	//inFile = openInputWaveFile(file_name, &sampleRate, &channel_num);
	//outFile = openOutputWaveFile(out_file_name, sampleRate, channel_num);



	stream = sonicCreateStream(sampleRate, channel_num);
	sonicSetSpeed(stream, speed);
	/*sonicSetPitch(stream, 1.0);
	sonicSetRate(stream, 1.0);*/
	int readByteSize = 0, samplesWritten = 0;
	//int ret = fread(inBuf, 1, BUFFER_SIZE, pFile);
	do {
		//memset(inBuf, 0, BUFFER_SIZE);
		readByteSize = fread(inBuf, 1, BUFFER_SIZE, pFile);
		int samples_num = readByteSize / channel_num / 2;

		if (readByteSize == 0) {
			sonicFlushStream(stream);
		}
		else {
			readSamples((short*)inBuf, sampleInBuffer, samples_num, 2);
			sonicWriteShortToStream(stream, sampleInBuffer, samples_num);
		}
		do {
			samplesWritten = sonicReadShortFromStream(stream, sampleOutBuffer, BUFFER_SIZE / channel_num / 2);
			//printf("%d\n", samplesWritten);
			if (samplesWritten > 0) {
				//memset(outBuf, 0, BUFFER_SIZE);
				writeSamples((short*)outBuf, sampleOutBuffer, samplesWritten, channel_num);
				fwrite(outBuf, 1, samplesWritten * 2 * 2, pOutFile);
			}
		} while (samplesWritten > 0);
	} while (readByteSize > 0);

	/*do {
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
			if (samplesWritten > 0) {
				writeToWaveFile(outFile, outBuffer, samplesWritten);
			}
		} while (samplesWritten > 0);
	} while (samplesRead > 0);*/
	fclose(pFile);
	fclose(pOutFile);
	sonicDestroyStream(stream);
	//closeWaveFile(inFile);
	//closeWaveFile(outFile);
	return 0;
}