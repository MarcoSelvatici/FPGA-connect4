#include <ap_fixed.h>
#include <ap_int.h>
#include <stdint.h>
#include <assert.h>

//#include <iostream>

// types:
typedef ap_uint<1> u1b;
typedef ap_uint<2> u2b;
typedef ap_uint<3> u3b;
typedef ap_uint<8> u8b;
typedef ap_int<8> s8b;
typedef ap_uint<10> u10b;
typedef ap_uint<11> u11b;
typedef ap_int<12> s12b;
typedef ap_uint<16> u16b;

#define RIGHT_COLUMN_PIXEL 1043
#define CORNER_DETECTION_MODE 0
#define COLOUR_DETECTION_MODE 1

// corner detection
#define MAX_QUEUE_SIZE 50
//#define THRESHOLD (MAX_QUEUE_SIZE * 3) * 80

// colour detection
#define SAMPLED_PIXELS 6
//#define COLOUR_THRESHOLD (140 * (2 * SAMPLED_PIXELS - 1))
#define WHITE 0
#define RED 1
#define GREEN 2

//u2b colours[42]; // 2 bits unsigned

struct pixel_queue{
	u10b queue[MAX_QUEUE_SIZE]; // ==> each value has range 0 -> 765 ==> 10 bits unsigned
	s8b head; // address of the current head -1 -> 49 ==> 8 bits signed
	u1b full;
	u16b curr_sum; // 0 -> 38250 ==> 16 bits

	pixel_queue(){
		head = -1;
		full = false;
		curr_sum = 0;
	}

	void addQ(uint8_t in_r, uint8_t in_g, uint8_t in_b){
		head = (head + 1) % MAX_QUEUE_SIZE;
		if(full){ // if full remove the least recent pixel
			curr_sum -= queue[head];
		}
		queue[head] = in_r + in_g + in_b; // overwrite the old pixel
		curr_sum += in_r + in_g + in_b;
		if(head == MAX_QUEUE_SIZE - 1){
			full = true;
		}
	}
};

void corner_colour_detection(volatile uint32_t* in_data,
					  volatile uint32_t* out_data,
					  u11b w, u11b h,
					  u1b mode,
					  s12b corners_coord[4], u16b THRESHOLD,
					  u2b colours_out[42], u16b COLOUR_THRESHOLD){

// for the global colours
//#pragma HLS ARRAY_PARTITION variable=colours complete dim=1

#pragma HLS INTERFACE s_axilite port=return
#pragma HLS INTERFACE s_axilite port=w
#pragma HLS INTERFACE s_axilite port=h

#pragma HLS INTERFACE m_axi depth=921600 port=in_data offset=slave // This will NOT work for resolutions higher than 1080p
#pragma HLS INTERFACE m_axi depth=921600 port=out_data offset=slave

// shared pragma
#pragma HLS INTERFACE s_axilite port=mode
#pragma HLS INTERFACE s_axilite port=corners_coord
#pragma HLS ARRAY_PARTITION variable=corners_coord complete dim=1

// pragma for corner detection parameters
#pragma HLS INTERFACE s_axilite port=THRESHOLD

// pragma for colour detection parameters
#pragma HLS INTERFACE s_axilite port=COLOUR_THRESHOLD
#pragma HLS INTERFACE s_axilite port=colours_out
#pragma HLS ARRAY_PARTITION variable=colours_out complete dim=1

// optimizations
#pragma HLS LOOP_FLATTEN off

	// ################
	// CORNER DETECTION
	// ################
	if(mode == CORNER_DETECTION_MODE){
		// array to return
		s12b corners_coordinates[4] = {1000,1000,  -1,-1}; // range -1 -> 1280 ==> 12 bits signed
		u1b corner_idx = 0; // range 0 -> 1 ==> 1 bit unsigned
		u1b found_first_corner = 0;

		pixel_queue queue;
//#pragma HLS ARRAY_PARTITION variable=queue.queue complete dim=1

		for (u11b i = 0; i < h; ++i) {
			for (u11b j = 0; j < w; ++j){
#pragma HLS PIPELINE II=3

				unsigned int current = *in_data++;

				// extract colors
				uint8_t in_r = current & 0xFF;
				uint8_t in_g = (current >> 8) & 0xFF;
				uint8_t in_b = (current >> 16) & 0xFF;

				uint8_t out_r = in_r;
				uint8_t out_b = in_b;
				uint8_t out_g = in_g;

				if(j >= RIGHT_COLUMN_PIXEL){
					out_r = 214;
					out_b = 0;
					out_g = 0;
					unsigned int output = out_r | (out_g << 8) | (out_b << 16);
					*out_data++ = output;
					continue;
				}

				queue.addQ(in_r, in_g, in_b);
				if(i > h/2 && corner_idx < 1){
					found_first_corner = 1;
					corner_idx++;
				}

				if(found_first_corner == 0){
					// look for top left pixel
					if(queue.full && queue.curr_sum < THRESHOLD && (corners_coordinates[0] + corners_coordinates[1] > i + j)){
						// found a row of 50 under the threshold, save corner
						// std::cout << "Corner " << corner_idx << " found_first_corner at: " << i << ", " << j - MAX_QUEUE_SIZE << std::endl;
						corners_coordinates[corner_idx*2]     = i;
						corners_coordinates[corner_idx*2 + 1] = j - MAX_QUEUE_SIZE;
					}
				}
				else{
					// look for bottom right pixel
					if(queue.full && queue.curr_sum < THRESHOLD && (corners_coordinates[2] + corners_coordinates[3] < i + j)){
						// found a row of 50 under the threshold, save corner
						//std::cout << "Corner " << corner_idx << " found_first_corner at: " << i << ", " << j << std::endl;
						corners_coordinates[corner_idx*2]     = i;
						corners_coordinates[corner_idx*2 + 1] = j;
					}
				}

				/*
				if((i == 46 && j == 151)||(i == 639 && j == 1136)){
					out_r = 255;
					out_g = 0;
					out_b = 0;
				}
				for(int x = 0; x < 42; x++){
					if(j == arr[2*x] && i == arr[2*x+1]){
						out_r = 255;
						out_g = 0;
						out_b = 0;
					}
				}*/


				unsigned int output = out_r | (out_g << 8) | (out_b << 16);

				*out_data++ = output;
			}
		}
		/*
		std::cout << "top left: " << corners_coordinates[0] << " " << corners_coordinates[1] << std::endl
				  << "bottom right: " << corners_coordinates[2] << " " << corners_coordinates[3] << std::endl;
		*/

		for(u3b i = 0; i < 4; i++){
			corners_coord[i] = corners_coordinates[i];
		}
	}
	// ################
	// COLOUR DETECTION
	// ################
	else{
		volatile uint32_t* in_data_b = in_data;

		u11b center_rows[6]; // each has to be like corners
		u11b center_cols[7]; // same
		// find centers
		u8b delta_y = (corners_coord[2] - corners_coord[0]) / 12; // 0 -> 91 ==> 7 bits unsigned
		u8b delta_x = (corners_coord[3] - corners_coord[1]) / 14; // same

		for(u3b row = 0; row < 6; row++){
			center_rows[row] =  2 * delta_y * row + delta_y + corners_coord[0];
		}
		for(u3b col = 0; col < 7; col++){
			center_cols[col] = 2 * delta_x * col + delta_x + corners_coord[1];
		}

		// array to return
		// 0 white
		// 2 green

		u3b col_idx = 0; // 0 -> 7 ==> 3 bits
		u3b row_idx = 0; // same
		u16b curr_sum_r = 0; // 16 bits
		u16b curr_sum_g = 0;
		u1b sampling = 0;

		for (u11b i = 0; i < h; ++i) {
			for (u11b j = 0; j < w; ++j){
//#pragma HLS LATENCY max=10
#pragma HLS PIPELINE II=1

				unsigned int current = *in_data++;

				if(j >= RIGHT_COLUMN_PIXEL){
					*out_data++ = uint8_t(255) | (uint8_t(0) << 8) | (uint8_t(0) << 16);;
					continue;
				}else{
					*out_data++ = current;
				}
			}
		}

		for(int i = 0; i < 6; i++){
			for(int j = 0; j < 7; j++){
#pragma HLS UNROLL factor=7
#pragma HLS PIPELINE II=1

				//std::cout << center_rows[i]*1280 << " " << center_cols[j] << std::endl;
				unsigned int curr = in_data_b[center_rows[i]*1280 + center_cols[j]];
				u8b red = curr & 0xFF;
				u8b green = (curr >> 8) & 0xFF;

				if(red > COLOUR_THRESHOLD && green > COLOUR_THRESHOLD)
					colours_out[i*7 + j] = 0;
				else if(red > green)
					colours_out[i*7 + j] = 1;
				else
					colours_out[i*7 + j] = 2;

				//std::cout << "\nHERE " << red << " " << green << " " << blue << "\n";
			}
			//std::cout <<std::endl;
		}
		//std::cout <<std::endl;

		/*
		for(u8b i = 0; i < 42; i++){
//#pragma HLS UNROLL factor=42
			colours_out[i] = colours[i];
		}
		*/
	}
}