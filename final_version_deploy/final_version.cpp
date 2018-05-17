#include <ap_fixed.h>
#include <ap_int.h>
#include <stdint.h>
#include <assert.h>

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
#define GREEN 1
#define RED 2

//u2b colours[42]; // 2 bits unsigned
u11b center_rows[6]; // each has to be like corners
u11b center_cols[7]; // same

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
					  u2b colours_out[42], u16b COLOUR_THRESHOLD, u11b bar_level){
// globals
#pragma HLS ARRAY_PARTITION variable=center_cols complete dim=1
#pragma HLS ARRAY_PARTITION variable=center_rows complete dim=1

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
#pragma HLS INTERFACE s_axilite port=bar_level

// optimizations
#pragma HLS LOOP_FLATTEN off

	// ################
	// CORNER DETECTION
	// ################
	if(mode == CORNER_DETECTION_MODE){
		// array to return
		corners_coord[0] = 1000;
		corners_coord[1] = 1000;
		corners_coord[2] = -1;
		corners_coord[3] = -1; // range -1 -> 1280 ==> 12 bits signed

		u1b corner_idx = 0; // range 0 -> 1 ==> 1 bit unsigned
		u1b found_first_corner = 0;

		pixel_queue queue;
//#pragma HLS ARRAY_PARTITION variable=queue.queue complete dim=1

		for (u11b i = 0; i < h; ++i) {
			for (u11b j = 0; j < w; ++j){
#pragma HLS PIPELINE II=3

				unsigned int current = *in_data++;

				if(j >= RIGHT_COLUMN_PIXEL){
					*out_data++ = 214;
					continue;
				}

				queue.addQ(current & 0xFF, (current >> 8) & 0xFF, (current >> 16) & 0xFF);
				if(i > h/2 && corner_idx < 1){
					found_first_corner = 1;
					corner_idx++;
				}

				if(found_first_corner == 0){
					// look for top left pixel
					if(queue.full && queue.curr_sum < THRESHOLD && (corners_coord[0] + corners_coord[1] > i + j)){
						// found a row of 50 under the threshold, save corner
						corners_coord[corner_idx*2]     = i;
						corners_coord[corner_idx*2 + 1] = j - MAX_QUEUE_SIZE;
					}
				}
				else{
					// look for bottom right pixel
					if(queue.full && queue.curr_sum < THRESHOLD && (corners_coord[2] + corners_coord[3] < i + j)){
						// found a row of 50 under the threshold, save corner
						corners_coord[corner_idx*2]     = i;
						corners_coord[corner_idx*2 + 1] = j;
					}
				}

				*out_data++ = current;
			}
		}


		// find centers
		u8b delta_y = (corners_coord[2] - corners_coord[0]) / 12; // 0 -> 91 ==> 7 bits unsigned
		u8b delta_x = (corners_coord[3] - corners_coord[1]) / 14; // same

		for(u3b row = 0; row < 6; row++){
#pragma HLS UNROLL factor=6
			center_rows[row] =  2 * delta_y * row + delta_y + corners_coord[0];
		}
		for(u3b col = 0; col < 7; col++){
#pragma HLS UNROLL factor=7
			center_cols[col] = 2 * delta_x * col + delta_x + corners_coord[1];
		}
	}

	// ################
	// COLOUR DETECTION
	// ################
	else{
		volatile uint32_t* in_data_b = in_data;
		u3b col_idx = 0; // range 0 - 6
		u3b row_idx = 0; // range 0 - 5

		// array to return
		// 0 white
		// 2 green

		for (u11b i = 0; i < h; ++i) {
			for (u11b j = 0; j < w; ++j){
#pragma HLS PIPELINE II=1

				unsigned int current = *in_data++;

				if(j >= RIGHT_COLUMN_PIXEL){
					if(i < bar_level){
						*out_data++ = 214;
					}
					else{
						*out_data++ = (214 << 8);
					}
				}
				else{
					if(row_idx < 6 && i == center_rows[row_idx] && j == center_cols[col_idx]){
						u8b red = current & 0xFF;
						u8b green = (current >> 8) & 0xFF;

						if(red > COLOUR_THRESHOLD && green > COLOUR_THRESHOLD)
							colours_out[row_idx*7 + col_idx] = WHITE;
						else if(red > green)
							colours_out[row_idx*7 + col_idx] = RED;
						else
							colours_out[row_idx*7 + col_idx] = GREEN;

						if(col_idx == 6){
							col_idx = 0;
							row_idx++;
						}
						else{
							col_idx++;
						}
					}
					*out_data++ = current;
				}
			}
		}
	}
}
