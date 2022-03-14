#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cstdint>
extern "C"
{
#include "jpeglib.h"
}

struct ImageData {
	unsigned char *pixels;
	int64_t width;
	int64_t height;
	char components;
};

ImageData *imageData;

unsigned char findIntensity(unsigned char *color)
{
	unsigned char intensity;
	if (imageData->components == 1)
		intensity = *color;
	else
	{
		unsigned char r = *color;
		unsigned char g = *(color + 1);
		unsigned char b = *(color + 2);
		intensity = (unsigned char)(0.299 * r + 0.587 * g + 0.114 * b);
	}
	return intensity;
}

int fileOutputJpeg(const char *fileOutput)
{
	int quality = 100;
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	FILE * outfile;
	fopen_s(&outfile, fileOutput, "wb");
	jpeg_stdio_dest(&cinfo, outfile);

	cinfo.image_width = imageData->width;
	cinfo.image_height = imageData->height;
	cinfo.input_components = imageData->components;

	if (imageData->components == 3)
		cinfo.in_color_space = JCS_RGB;         // Цветное изображение (RGB)
	else if (imageData->components == 1)
		cinfo.in_color_space = JCS_GRAYSCALE;	// Изображение в градациях серого
	else
	{
		printf("Something wrong(fileOutputJpeg)\n");
		return 2;
	}

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, true);
	jpeg_start_compress(&cinfo, TRUE);

	JSAMPROW row_pointer[1];
	int row_stride = cinfo.image_width * imageData->components;
	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = (JSAMPLE *)(imageData->pixels + cinfo.next_scanline * row_stride);
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	fclose(outfile);
	return 0;
}

int fileInputJpeg(const char *fileInput)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	int row_stride;

	FILE * infile;
	fopen_s(&infile, fileInput, "rb");
	if (!infile)
	{
		printf("Can't open %s", fileInput);
		return 1;
	}

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, infile);
	jpeg_read_header(&cinfo, TRUE);
	jpeg_start_decompress(&cinfo);

	row_stride = cinfo.output_width * cinfo.output_components;

	JSAMPARRAY buffer = (JSAMPARRAY)malloc(sizeof(JSAMPROW));
	buffer[0] = (JSAMPROW)malloc(sizeof(JSAMPLE) * row_stride);

	imageData = new ImageData;
	imageData->width = cinfo.output_width;
	imageData->height = cinfo.output_height;
	imageData->components = cinfo.output_components;
	imageData->pixels = new unsigned char[cinfo.output_width * cinfo.output_height * cinfo.output_components];

	long counter = 0;
	while (cinfo.output_scanline < cinfo.output_height) {
		buffer[0] = imageData->pixels + counter;
		jpeg_read_scanlines(&cinfo, buffer, 1);
		counter += row_stride;
	}

	/* Finish decompression */
	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	fclose(infile);
	return 0;
}

int * createIntensityHist(int countIntensLevels)
{
	int *hist = new int[countIntensLevels];
	for (int i = 0; i < countIntensLevels; i++)
		hist[i] = 0;

	for (int i = 0; i < imageData->width; i++)
		for (int j = 0; j < imageData->height; j++)
		{
			unsigned char *color = imageData->pixels + j * imageData->width * imageData->components + i * imageData->components;
			unsigned char intensity = findIntensity(color);

			hist[intensity]++;
		}
	return hist;
}

int otsu()
{
	int intensLevels = 256;
	int *hist = createIntensityHist(intensLevels);
	long pixelsCount0 = 0;
	long allPixelsCount = imageData->width * imageData->height;
	long intensitySum0 = 0;

	long allIntensitySum = 0;
	for (int i = 0; i < intensLevels; i++)
		allIntensitySum += i * hist[i];

	double maxRes = 0;
	int threshold = 1;
	for (int k = 1; k < intensLevels; k++)
	{
		pixelsCount0 += hist[k - 1];
		intensitySum0 += hist[k - 1] * (k - 1);

		long w0 = pixelsCount0;
		long w1 = allPixelsCount - w0;

		double m0 = (double)intensitySum0 / w0;
		double m1 = (double)(allIntensitySum - intensitySum0) / w1;

		double betweenClassVariance = (double)w0 * (double)w1 * (m1 - m0) * (m1 - m0);

		if (betweenClassVariance > maxRes)
		{
			maxRes = betweenClassVariance;
			threshold = k;
		}
	}
	return threshold;
}

void binarization(int threshold)
{
	for (int i = 0; i < imageData->width; i++)
		for (int j = 0; j < imageData->height; j++)
		{
			unsigned char *color = imageData->pixels + j * imageData->width * imageData->components + i * imageData->components;
			unsigned char intensity = findIntensity(color);
			unsigned char newColor = 255;

			if (intensity < threshold)
				newColor = 0;

			for (int k = 0; k < imageData->components; k++)
			{
				*color = newColor;
				color++;
			}
		}
}

int main()
{
	printf("Started");

	const char *inFile = "img6.jpg";

	int err = fileInputJpeg(inFile);
	if (err == 1)
		return err;
	int threshold = otsu();
	binarization(threshold);

	char outFile[512];
	snprintf(outFile, strlen(outFile), "%s%s", "Result_", inFile);
	err = fileOutputJpeg(outFile);
	if (err == 2)
		return err;

	free(imageData);
	return 0;
}