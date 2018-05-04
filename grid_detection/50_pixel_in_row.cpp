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

#define THRESHOLD  75

struct pixel_data {
	pixel_type blue;
	pixel_type green;
	pixel_type red;
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
	int corners_coordinates[4] = {-1,-1,  -1,-1};
	int corner_idx = 0;
	bool found_first_corner = false;
	int pixel_row_len = 0;

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

			if(!found_first_corner){
				// look for top left pixel
				if(is_black(in_r, in_g, in_b)){
					if(pixel_row_len < 50){
						// keep on counting for the row
						pixel_row_len++;
					}
					else{
						// found a row of 50, save corner
						found_first_corner = true;
						std::cout << "Corner " << corner_idx << " found_first_corner at: " << i << ", " << j - 50 << std::endl;
						corners_coordinates[corner_idx*2]     = i;
						corners_coordinates[corner_idx*2 + 1] = j - 50;
						corner_idx++;
					}
				}
				else{
					// not black pixel
					pixel_row_len = 0;
				}
			}
			else{
				// look for bottom right pixel
				if(is_black(in_r, in_g, in_b)){
					if(pixel_row_len > 0){
						// keep on counting to get to zero
						pixel_row_len--;
					}
					else{
						// found a row of 50, save corner
						pixel_row_len = 50;
						std::cout << "Corner " << corner_idx << " found_first_corner at: " << i << ", " << j << std::endl;
						corners_coordinates[corner_idx*2]     = i;
						corners_coordinates[corner_idx*2 + 1] = j;
					}
				}
				else{
					// not a black pixel
					pixel_row_len = 50;
				}
			}

			unsigned int output = out_r | (out_g << 8) | (out_b << 16);

			*out_data++ = output;
		}
	}
	
	// detected on the "wrong" side of the image
	if(corners_coordinates[1] > w/2){
		corners_coordinates[1] += 50;
		corners_coordinates[3] -= 50;
	}
	
	std::cout << "top left: " << corners_coordinates[0] << " " << corners_coordinates[1] << std::endl
			  << "bottom right: " << corners_coordinates[2] << " " << corners_coordinates[3] << std::endl;
}

