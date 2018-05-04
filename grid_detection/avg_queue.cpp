#include <ap_fixed.h>
#include <ap_int.h>
#include <stdint.h>
#include <assert.h>

#include <iostream> // debug

typedef ap_uint<8> pixel_type;
typedef ap_int<8> pixel_type_s;
typedef ap_uint<96> u96b;
typedef ap_uint<32> word_32;
typedef ap_ufixed<8,0, AP_RND, AP_SAT> comp_type;
typedef ap_fixed<10,2, AP_RND, AP_SAT> coeff_type;

#define MAX_QUEUE_SIZE 100
#define THRESHOLD ( MAX_QUEUE_SIZE * 3) * 70

struct pixel_data{
	pixel_type blue;
	pixel_type green;
	pixel_type red;
};

// todo: should add interfaces for full and curr_sum?
struct pixel_queue{
	int queue[MAX_QUEUE_SIZE];
	int head; // address of the current head
	bool full;
	int curr_sum;

	pixel_queue(){
		head = -1;
		full = false;
		curr_sum = 0;
	}

	/*
	int size(){
		if(full)
			return MAX_QUEUE_SIZE;
		return head + 1;
	}
	*/

	void add(uint8_t in_r, uint8_t in_g, uint8_t in_b){
		head = (head + 1) % MAX_QUEUE_SIZE;
		if(full){ // if full remove the last recent pixel
			curr_sum -= queue[head];
		}
		queue[head] = in_r + in_g + in_b; // overwrite the old pixel
		curr_sum += in_r + in_g + in_b;
		if(head == MAX_QUEUE_SIZE - 1){
			full = true;
		}
	}
};

bool is_black(uint8_t in_r, uint8_t in_g, uint8_t in_b){
	return (in_g < THRESHOLD && in_r < THRESHOLD && in_b < THRESHOLD);
}

void corner_detection(volatile uint32_t* in_data, volatile uint32_t* out_data, int w, int h, int parameter_1){
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS INTERFACE s_axilite port=parameter_1
#pragma HLS INTERFACE s_axilite port=w
#pragma HLS INTERFACE s_axilite port=h

#pragma HLS INTERFACE m_axi depth=2073600 port=in_data offset=slave // This will NOT work for resolutions higher than 1080p
#pragma HLS INTERFACE m_axi depth=2073600 port=out_data offset=slave


#pragma HLS PIPELINE II=1
#pragma HLS LOOP_FLATTEN off

	// array to return
	int16_t corners_coordinates[4] = {-1,-1,  -1,-1}; // range -1 -> 1920
	int corner_idx = 0; // range 0 -> 1
	bool found_first_corner = false;
	int pixel_row_len = 0; // range 0 -> 50
	pixel_queue queue;

	volatile uint32_t* tmp_in_data = in_data;

	for (int i = 0; i < h; ++i) {
		for (int j = 0; j < w; ++j){
			unsigned int current = *in_data++;

			// extract colors
			uint8_t in_r = current & 0xFF;
			uint8_t in_g = (current >> 8) & 0xFF;
			uint8_t in_b = (current >> 16) & 0xFF;

			uint8_t out_r = in_r;
			uint8_t out_b = in_b;
			uint8_t out_g = in_g;

			queue.add(in_r, in_g, in_b);

			if(!found_first_corner){
				// look for top left pixel
				if(queue.full && queue.curr_sum < THRESHOLD){
					// found a row of 50 under the threshold, save corner
					found_first_corner = true;
					std::cout << "Corner " << corner_idx << " found_first_corner at: " << i << ", " << j - 50 << std::endl;
					corners_coordinates[corner_idx*2]     = i;
					corners_coordinates[corner_idx*2 + 1] = j - 50;
					corner_idx++;
				}
			}
			else{
				// look for bottom right pixel
				if(queue.full && queue.curr_sum < THRESHOLD){
					// found a row of 50 under the threshold, save corner
					std::cout << "Corner " << corner_idx << " found_first_corner at: " << i << ", " << j << std::endl;
					corners_coordinates[corner_idx*2]     = i;
					corners_coordinates[corner_idx*2 + 1] = j;
				}
			}

            /*
			if((i == 143 && j == 1530)||(i == 888 && j == 504)){
				out_r = 255;
				out_g = 0;
				out_b = 0;
			}
            */

			unsigned int output = out_r | (out_g << 8) | (out_b << 16);

			*out_data++ = output;
		}
	}

	std::cout << "top left: " << corners_coordinates[0] << " " << corners_coordinates[1] << std::endl
			  << "bottom right: " << corners_coordinates[2] << " " << corners_coordinates[3] << std::endl;

}

