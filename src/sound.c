#include "sound.h"
#include "memory.h"
#include "SDL2/SDL.h"
#include <math.h>

const double duty_table[4] = {0.125, 0.25, 0.5, 0.75};

struct rect_channel {
	int sweep_diff;
	int sweep_dir;
	int sweep_time;
	int sweep_steps;
	int length;
	int duty_num;
	int envelope_time;
	int envelope_dir;
	int envelope_init_volume;
	int envelope_steps;
	int freq;
	int counter_enabled;
	int right_enabled;
	int left_enabled;
	int status;
	int prev;
	int sweep_countdown;
	int volume;
	int restart;
	unsigned int step;
	int remaining_time; //ms
	int ms_countdown;
};

struct wave_channel {
	int enabled;
	int length;
	int volume_ratio;
	int freq;
	int counter_enabled;
	int right_enabled;
	int left_enabled;
	int status;
	unsigned int step;
	int remaining_time;
	int restart;
	int ms_countdown;
	int wave_index;
};

struct noise_channel {
	int length;
	int envelope_time;
	int envelope_dir;
	int envelope_init_volume;
	int ratio;
	int shiftclk_freq;
	int cycle;
	int counter_enabled;
	int right_enabled;
	int left_enabled;
	int status;
	unsigned int step;
	int restart;
	int volume;
	int gen_steps;
	uint16_t shiftreg;
	int prng_out;
	int envelope_steps;
	int remaining_time;
	int ms_countdown;
};

struct master_volume {
	int right_volume;
	int left_volume;
	int right_enabled;
	int left_enabled;
	int all_enabled;
};


static SDL_AudioSpec Desired;
static SDL_AudioSpec Obtained;

static struct rect_channel ch1, ch2;
static struct wave_channel ch3;
static struct noise_channel ch4;
static struct master_volume master;

#define REAL_FREQ(freq) (131072.0 / (2048.0 - (freq)))

static double rectwave(double t, double duty) {
  return t - abs(t) < duty ? 1 : -1;
}


void sound_ch1_writereg(uint16_t ioreg, uint8_t value) {
	switch(ioreg){
	case IO_NR10_R:
		ch1.sweep_diff=value&0x7;
		ch1.sweep_dir=value>>3&0x1;
		ch1.sweep_time=value>>4;
		break;
	case IO_NR11_R:
		ch1.length=value&0x3f;
		ch1.duty_num=value>>6;
		break;
	case IO_NR12_R:
		ch1.envelope_time=value&0x7;
		ch1.envelope_dir=value>>3&0x1;
		ch1.envelope_init_volume=value>>4;
		break;
	case IO_NR13_R:
		ch1.freq=(ch1.freq&0x700)|value;
		break;
	case IO_NR14_R:
		ch1.freq=(ch1.freq&0xff)|((value&0x7)<<8);
		ch1.counter_enabled=value>>6&0x1;
		ch1.restart=value>>7;
		break;
	}
}

void sound_ch2_writereg(uint16_t ioreg, uint8_t value) {
	switch(ioreg){
	case IO_NR21_R:
		ch2.length=value&0x3f;
		ch2.duty_num=value>>6;
		break;
	case IO_NR22_R:
		ch2.envelope_time=value&0x7;
		ch2.envelope_dir=value>>3&0x1;
		ch2.envelope_init_volume=value>>4;
		break;
	case IO_NR23_R:
		ch2.freq=(ch2.freq&0x700)|value;
		break;
	case IO_NR24_R:
		ch2.freq=(ch2.freq&0xff)|((value&0x7)<<8);
		ch2.counter_enabled=value>>6&0x1;
		ch2.restart=value>>7;
		break;
	}
}

void sound_ch3_writereg(uint16_t ioreg, uint8_t value) {
	switch(ioreg){
	case IO_NR30_R:
		ch3.enabled = value>>7;
		break;
	case IO_NR31_R:
		ch3.length=value;
		break;
	case IO_NR32_R:
		ch3.volume_ratio=(value>>5)&0x3;
		break;
	case IO_NR33_R:
		ch3.freq=(ch3.freq&0x700)|value;
		break;
	case IO_NR34_R:
		ch3.freq=(ch3.freq&0xff)|((value&0x7)<<8);
		ch3.counter_enabled=value>>6&0x1;
		ch3.restart=value>>7;
		break;
	}
}

void sound_ch4_writereg(uint16_t ioreg, uint8_t value) {
	switch(ioreg){
	case IO_NR41_R:
		ch4.length=value&0x3f;
		break;
	case IO_NR42_R:
		ch4.envelope_time=value&0x7;
		ch4.envelope_dir=value>>3&0x1;
		ch4.envelope_init_volume=value>>4;
		break;
	case IO_NR43_R:
		ch4.ratio=value&0x7;
		ch4.cycle=value>>3&0x1;
		ch4.shiftclk_freq=value>>4;
		break;
	case IO_NR44_R:
		ch4.counter_enabled=value>>6&0x1;
		ch4.restart=value>>7;
		break;
	}
}

void sound_master_writereg(uint16_t ioreg, uint8_t value) {
	switch(ioreg){
	case IO_NR50_R:
		master.right_volume=value&0x7;
		master.right_enabled=value>>3&0x1;
		master.left_volume=value>>4&0x7;
		master.left_enabled=value>>7;
		break;
	case IO_NR51_R:
        ch1.right_enabled=value&0x1;
        ch2.right_enabled=value>>1&0x1;
        ch3.right_enabled=value>>2&0x1;
        ch4.right_enabled=value>>3&0x1;
        ch1.left_enabled=value>>4&0x1;
        ch2.left_enabled=value>>5&0x1;
        ch3.left_enabled=value>>6&0x1;
        ch4.left_enabled=value>>7&0x1;
		break;
	case IO_NR52_R:
		master.all_enabled=value>>7;
		break;
	}
}

uint8_t sound_ch1_readreg(uint16_t ioreg) {
	switch(ioreg){
	case IO_NR10_R:
		return ch1.sweep_diff | ch1.sweep_dir<<3 | ch1.sweep_time<<4;
	case IO_NR11_R:
		return ch1.duty_num<<6;
	case IO_NR12_R:
		return ch1.envelope_time | ch1.envelope_dir<<3 | ch1.envelope_init_volume<<4;
	case IO_NR13_R:
		return 0; //write only
	case IO_NR14_R:
		return ch1.counter_enabled<<6;
	}

	return 0;
}

uint8_t sound_ch2_readreg(uint16_t ioreg) {
	switch(ioreg){
	case IO_NR21_R:
		return ch2.duty_num<<6;
	case IO_NR22_R:
		return ch2.envelope_time | ch2.envelope_dir<<3 | ch2.envelope_init_volume<<4;
	case IO_NR23_R:
		return 0; //write only
	case IO_NR24_R:
		return ch2.counter_enabled<<6;
	}

	return 0;
}

uint8_t sound_ch3_readreg(uint16_t ioreg) {
	switch(ioreg){
	case IO_NR31_R:
		return ch3.enabled<<7;
	case IO_NR32_R:
		return ch3.volume_ratio<<5;
	case IO_NR33_R:
		return 0; //write only
	case IO_NR34_R:
		return ch3.counter_enabled<<6;
	}

	return 0;
}

uint8_t sound_ch4_readreg(uint16_t ioreg) {
	switch(ioreg){
	case IO_NR42_R:
		return ch4.envelope_time | ch4.envelope_dir<<3 | ch4.envelope_init_volume<<4;
	case IO_NR43_R:
		return ch4.shiftclk_freq | ch4.cycle<<3 | ch4.ratio<<4;
	case IO_NR44_R:
		return ch4.counter_enabled<<6;
	}

	return 0;
}

uint8_t sound_master_readreg(uint16_t ioreg) {
	switch(ioreg){
	case IO_NR50_R:
		return master.right_volume | master.right_enabled<<3 | master.left_volume<<4 | master.left_enabled<<7;
	case IO_NR51_R:
        return ch1.right_enabled | ch2.right_enabled<<1 | ch3.right_enabled<<2 | ch4.right_enabled<<3 |
				ch1.left_enabled<<4 | ch2.left_enabled<<5 | ch3.left_enabled<<6 | ch4.left_enabled<<7;
	case IO_NR52_R:
		return ch1.status | ch2.status<<1 | ch3.status<<2 | ch4.status<<3 | master.all_enabled<<7;
	}

	return 0;
}

static int ch1_wave() {
	if(ch1.restart){
		ch1.volume = ch1.envelope_init_volume;
		ch1.sweep_steps = Obtained.freq*ch1.sweep_time / 128;
		ch1.envelope_steps = Obtained.freq*ch1.envelope_time / 64;
		ch1.sweep_countdown = ch1.sweep_steps;
		ch1.restart = 0;
		ch1.status = 1;
		ch1.prev = -1;
		if(ch1.counter_enabled)
			ch1.remaining_time = (64-ch1.length)*1000/256;
		ch1.ms_countdown = Obtained.freq / 1000; //1ms経過までのステップ数
	}

	int is_edge = ch1.prev != rectwave(ch1.step * REAL_FREQ(ch1.freq)/Obtained.freq, duty_table[ch1.duty_num]);
	//printf("sweep_time=%d, cd=%d, edge=%d, prev=%d\n", ch1.sweep_time, ch1.sweep_countdown, is_edge, ch1.prev);
	if(ch1.sweep_time!=0 && ch1.sweep_countdown==0 && is_edge && ch1.prev==-1){
		if(ch1.sweep_dir == 0 && ch1.freq+(ch1.freq>>ch1.sweep_diff)>2048){
			ch1.status = 0;
		}else if(!(ch1.sweep_dir == 1 && ch1.freq>>ch1.sweep_diff<0)){
			ch1.freq += (ch1.sweep_dir?-1:1) * (ch1.freq>>ch1.sweep_diff);
			//printf("freq = %d, real=%lf\n", ch1.freq, REAL_FREQ(ch1.freq));
			ch1.sweep_countdown = ch1.sweep_steps;
			ch1.step=0;
		}
	}
	if(ch1.envelope_time!=0 && ch1.envelope_steps!=0 && ch1.step%ch1.envelope_steps == 0){
		if(ch1.envelope_dir==1){
			if(ch1.volume<15) ch1.volume++;
		}else{
			if(ch1.volume>0) ch1.volume--;
		}
	}

	ch1.prev = rectwave(ch1.step++ * REAL_FREQ(ch1.freq) / Obtained.freq, duty_table[ch1.duty_num]);

	if(ch1.sweep_countdown>0)
		ch1.sweep_countdown--;

	if(ch1.ms_countdown>0){
		if(--ch1.ms_countdown == 0){
			ch1.ms_countdown = Obtained.freq/1000;
			ch1.remaining_time--;
		}
	}
	if(ch1.counter_enabled && ch1.remaining_time == 0)
		ch1.status = 0;

	if(!ch1.status)
		return 0;

	return ch1.prev * ch1.volume;
}

static int ch2_wave() {
	if(ch2.restart){
		ch2.volume = ch2.envelope_init_volume;
		ch2.envelope_steps = Desired.freq*ch2.envelope_time / 64;
		ch2.restart = 0;
		ch2.status = 1;
		if(ch2.counter_enabled)
			ch2.remaining_time = (64-ch2.length)*1000/256;
		ch2.ms_countdown = Obtained.freq / 1000; //1ms経過までのステップ数
	}

	if(ch2.envelope_time!=0 && ch2.envelope_steps!=0 && ch2.step%ch2.envelope_steps == 0){
		if(ch2.envelope_dir==1){
			if(ch2.volume<15) ch2.volume++;
		}else{
			if(ch2.volume>0) ch2.volume--;
		}
	}

	if(ch2.ms_countdown>0){
		if(--ch2.ms_countdown == 0){
			ch2.ms_countdown = Obtained.freq/1000;
			ch2.remaining_time--;
		}
	}

	if(ch2.counter_enabled && ch2.remaining_time == 0)
		ch2.status = 0;

	if(!ch2.status)
		return 0;

	return rectwave(ch2.step++ * REAL_FREQ(ch2.freq) / Obtained.freq, duty_table[ch2.duty_num]) * ch2.volume;
}

static int ch3_wave() {
	if(ch3.restart){
		ch3.restart = 0;
		ch3.status = 1;
		if(ch3.counter_enabled)
			ch3.remaining_time = (256-ch3.length)*1000/256;
		ch3.ms_countdown = Obtained.freq / 1000; //1ms経過までのステップ数
		ch3.wave_index = 0;
	}

	int frame;

	ch3.wave_index = fmod(ch3.step * ((65535.0/(2048.0-ch3.freq))*32.0) / Obtained.freq, 32);

	if(ch3.wave_index%2)
		frame = INTERNAL_IO[IO_WAVERAM_BEGIN_R+ch3.wave_index/2]&0xf;
	else
		frame = INTERNAL_IO[IO_WAVERAM_BEGIN_R+ch3.wave_index/2]>>4;

	ch3.step++;

	if(ch3.ms_countdown>0){
		if(--ch3.ms_countdown == 0){
			ch3.ms_countdown = Obtained.freq/1000;
			ch3.remaining_time--;
		}
	}

	if(ch3.counter_enabled && ch3.remaining_time == 0)
		ch3.status = 0;

	if(!ch3.status)
		return 0;

	return ch3.volume_ratio&&ch3.enabled ? frame >> (ch3.volume_ratio-1) : 0;
}


static int ch4_wave() {
	if(ch4.restart){
		ch4.volume = ch4.envelope_init_volume;
		ch4.envelope_steps = Desired.freq*ch4.envelope_time / 64;
		ch4.gen_steps = Desired.freq / (524288.0/(ch4.ratio?ch4.ratio:0.5)/pow(2, ch4.shiftclk_freq+1));
		if(ch4.gen_steps==0) ch4.gen_steps++;
		ch4.restart = 0;
		ch4.status = 1;
		if(ch4.counter_enabled)
			ch4.remaining_time = (64-ch4.length)*1000/256;
		ch4.ms_countdown = Obtained.freq / 1000; //1ms経過までのステップ数
		ch4.shiftreg = 0xffff;
		ch4.prng_out = 1;
	}

	if(ch4.step!=0 && ch4.envelope_steps!=0 && ch4.step%ch4.envelope_steps == 0){
		if(ch4.envelope_dir==1){
			if(ch4.volume<15) ch4.volume++;
		}else{
			if(ch4.volume>0) ch4.volume--;
		}
	}

	if(ch4.ms_countdown>0){
		if(--ch4.ms_countdown == 0){
			ch4.ms_countdown = Obtained.freq/1000;
			ch4.remaining_time--;
		}
	}

	if(ch4.counter_enabled && ch4.remaining_time == 0)
		ch4.status = 0;

	if(!ch4.status)
		return 0;

	if(ch4.step!=0 && ch4.gen_steps!=0 && ch4.step%ch4.gen_steps == 0){
		//擬似乱数の更新
		ch4.shiftreg = (ch4.shiftreg<<1) + (((ch4.shiftreg>>(ch4.cycle?6:14))^(ch4.shiftreg>>(ch4.cycle?5:13)))&1);
		ch4.prng_out ^= ch4.shiftreg&1;
	}

	ch4.step++;

	return ch4.volume * ch4.prng_out;
}

static void callback(void *unused, Uint8 *stream, int len) {
	Sint16 *frames = (Sint16 *) stream;
	int framesize = len / 2;
	for (int i = 0; i < framesize; i+=2) {
		int ch1_val=ch1_wave()*32, ch2_val=ch2_wave()*32, ch3_val=ch3_wave()*32, ch4_val=ch4_wave()*32;
        frames[i] = master.all_enabled*(ch1.left_enabled*ch1_val+ch2.left_enabled*ch2_val+ch3.left_enabled*ch3_val+ch4.left_enabled*ch4_val);
        frames[i] = frames[i] + (frames[i] * master.left_enabled);
        frames[i+1] = master.all_enabled*(ch1.right_enabled*ch1_val+ch2.right_enabled*ch2_val+ch3.right_enabled*ch3_val+ch4.right_enabled*ch4_val);
		frames[i+1] = frames[i+1] + (frames[i+1] * master.right_enabled);
	}
}

void sound_init() {
	Desired.freq= 44100;
	Desired.format= AUDIO_S16LSB;
	Desired.channels= 2;
	Desired.samples= 2048;
	Desired.callback= callback;
	Desired.userdata= NULL;
/*
	ch1.sweep_time=7;
	ch1.sweep_dir=0;
	ch1.sweep_diff=7;
	ch1.envelope_time=0;
	ch1.freq = 0x6e7;
	ch1.envelope_init_volume = 16;
	ch1.restart=1;
*/
	SDL_OpenAudio(&Desired, &Obtained);
	SDL_PauseAudio(0);

	return;
}
