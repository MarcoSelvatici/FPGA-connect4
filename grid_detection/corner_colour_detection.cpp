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

#define MAX_QUEUE_SIZE 50
#define THRESHOLD (MAX_QUEUE_SIZE * 3) * 70

struct pixel_data{
	pixel_type blue;
	pixel_type green;
	pixel_type red;
};

// to do: should add interfaces for full and curr_sum?
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

	int arr[84] = {221, 95, 361, 95, 501, 95, 641, 95, 781, 95, 921, 95, 1061, 95, 221, 193, 361, 193, 501, 193, 641, 193, 781, 193, 921, 193, 1061, 193, 221, 291, 361, 291, 501, 291, 641, 291, 781, 291, 921, 291, 1061, 291, 221, 389, 361, 389, 501, 389, 641, 389, 781, 389, 921, 389, 1061, 389, 221, 487, 361, 487, 501, 487, 641, 487, 781, 487, 921, 487, 1061, 487, 221, 585, 361, 585, 501, 585, 641, 585, 781, 585, 921, 585, 1061, 585};

	// array to return
	int16_t corners_coordinates[4] = {1000,1000,  -1,-1}; // range -1 -> 1920
	int corner_idx = 0; // range 0 -> 1
	bool found_first_corner = false;
	int pixel_row_len = 0; // range 0 -> 50
	pixel_queue queue;

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
			if(i > h/2 && corner_idx < 1){
				found_first_corner = true;
				corner_idx++;
			}

			if(!found_first_corner){
				// look for top left pixel
				if(queue.full && queue.curr_sum < THRESHOLD && (corners_coordinates[0] + corners_coordinates[1] > i + j)){
					// found a row of 50 under the threshold, save corner
					std::cout << "Corner " << corner_idx << " found_first_corner at: " << i << ", " << j - MAX_QUEUE_SIZE << std::endl;
					corners_coordinates[corner_idx*2]     = i + 5;
					corners_coordinates[corner_idx*2 + 1] = j - MAX_QUEUE_SIZE;
				}
			}
			else{
				// look for bottom right pixel
				if(queue.full && queue.curr_sum < THRESHOLD && (corners_coordinates[2] + corners_coordinates[3] < i + j)){
					// found a row of 50 under the threshold, save corner
					std::cout << "Corner " << corner_idx << " found_first_corner at: " << i << ", " << j << std::endl;
					corners_coordinates[corner_idx*2]     = i - 5;
					corners_coordinates[corner_idx*2 + 1] = j;
				}
			}


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
			}


			unsigned int output = out_r | (out_g << 8) | (out_b << 16);

			*out_data++ = output;
		}
	}

	std::cout << "top left: " << corners_coordinates[0] << " " << corners_coordinates[1] << std::endl
			  << "bottom right: " << corners_coordinates[2] << " " << corners_coordinates[3] << std::endl;

	int delta_y = (corners_coordinates[2] - corners_coordinates[0]) / 12;
	int delta_x = (corners_coordinates[3] - corners_coordinates[1]) / 14;

	for(int i = 0; i < 6; i++){
		for(int j = 0; j < 7; j++){
			/*std::cout << "\n" << i << " " << j;
			std::cout << "\nx coord: " << 2 * delta_x * i + delta_x + corners_coordinates[0]
					  << "\ny coord: " << 2 * delta_y * j + delta_y + corners_coordinates[1] << std::endl;*/
			std::cout << 2 * delta_x * j + delta_x + corners_coordinates[1] << ", " << 2 * delta_y * i + delta_y + corners_coordinates[0] << ", ";
		}
	}
}

#define SAMPLED_PIXELS 6
#define COLOUR_THRESHOLD (140 * (2 * SAMPLED_PIXELS - 1))
#define WHITE 0
#define RED 1
#define GREEN 2

void colour_detection(volatile uint32_t* in_data, volatile uint32_t* out_data, int w, int h, int parameter_1){
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS INTERFACE s_axilite port=parameter_1
#pragma HLS INTERFACE s_axilite port=w
#pragma HLS INTERFACE s_axilite port=h

#pragma HLS INTERFACE m_axi depth=2073600 port=in_data offset=slave // This will NOT work for resolutions higher than 1080p
#pragma HLS INTERFACE m_axi depth=2073600 port=out_data offset=slave


#pragma HLS PIPELINE II=1
#pragma HLS LOOP_FLATTEN off

	int centers_coordinates[84] = {221, 95, 361, 95, 501, 95, 641, 95, 781, 95, 921, 95, 1061, 95, 221, 193, 361, 193, 501, 193, 641, 193, 781, 193, 921, 193, 1061, 193, 221, 291, 361, 291, 501, 291, 641, 291, 781, 291, 921, 291, 1061, 291, 221, 389, 361, 389, 501, 389, 641, 389, 781, 389, 921, 389, 1061, 389, 221, 487, 361, 487, 501, 487, 641, 487, 781, 487, 921, 487, 1061, 487, 221, 585, 361, 585, 501, 585, 641, 585, 781, 585, 921, 585, 1061, 585};

	// array to return
	// 0 white
	// 2 green

	int center_colors[42] = {3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3};
	int center_idx = 0;
	int curr_sum_r = 0;
	int curr_sum_g = 0;
	bool sampling = false;

	for (int i = 0; i < h; ++i) {
		for (int j = 0; j < w; ++j){
			unsigned int current = *in_data++;

			if(sampling){
				if(j == centers_coordinates[2 * center_idx] + SAMPLED_PIXELS){
					// checks
					if(curr_sum_r > COLOUR_THRESHOLD && curr_sum_g > COLOUR_THRESHOLD){
						center_colors[center_idx] = WHITE;
					}else if(curr_sum_r > curr_sum_g){
						center_colors[center_idx] = RED;
					}else{
						center_colors[center_idx] = GREEN;
					}

					center_idx++;
					// reset everything
					curr_sum_r = 0;
					curr_sum_g = 0;
					sampling = false;
				}else{
					// extract colors
					curr_sum_r += current & 0xFF;
					curr_sum_g += (current >> 8) & 0xFF;
				}
			}else if((i == centers_coordinates[2*center_idx + 1]) && (j == centers_coordinates[2 * center_idx] - SAMPLED_PIXELS)){
				std::cout << "center_idx: " << center_idx << "\nred: " << (current & 0xFF)
														  << "\ngreen: " << ((current >> 8) & 0xFF)
														  << "\nblue: " << ((current >> 16) & 0xFF) << "\n";
				sampling = true;
			}

			unsigned int output = current;

			*out_data++ = output;
		}
	}

	for(int i = 0; i < 6; i++){
		for(int j = 0; j < 7; j++){
			std::cout << center_colors[i * 7 + j] <<" ";
		}
		std::cout << std::endl;
	}
}
