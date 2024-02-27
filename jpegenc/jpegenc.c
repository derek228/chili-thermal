#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>

#define WIDTH 80
#define HEIGHT 62
#define JPEG 1
// 定义RGB颜色结构体
typedef struct {
    unsigned char r, g, b;
} RGBColor;

// 插值函数，用于计算两个颜色之间的插值
unsigned char interpolate(unsigned char color1, unsigned char color2, double ratio) {
	if (ratio <0) 
		return (unsigned char)(color2);
	else if (ratio >1)
		return (unsigned char)(color1);
	else
	    return (unsigned char)(color1 * (ratio) + color2 * (1-ratio));
//    return (unsigned char)(color1 * (1 - ratio) + color2 * ratio);
}

// 根据温度值计算颜色
RGBColor temperature_to_color(unsigned int temperature) {
    // 根据温度将红色和蓝色进行插值
    double ratio = ((double)temperature - 3080) / 345;//  ((double)3080 - 2735); // 在308K和273K之间插值
    RGBColor color;
#if 0
    color.r = interpolate(255, 0, ratio); // 红色渐变
    color.g = interpolate(0, 255, ratio);// 绿色为0
    color.b =  0; // 蓝色渐变
#else
    color.r = interpolate(255, 0, ratio); // 红色渐变
    color.g = 0; // 绿色为0
    color.b = interpolate(0, 255, ratio); // 蓝色渐变
#endif
//	printf("temp=%d, ratio = %f, r=%d, b=%d\n", temperature, ratio,color.r, color.b);
    return color;
}

int main() {
    // 假设你已经有一个80x62的凯氏温度矩阵
	int i,j;


#ifdef JPEG
	int x,y;
    int temperature[HEIGHT][WIDTH];
	for (i=0;i<HEIGHT;i++) {
		for (j=0;j<WIDTH;j++) {
			temperature[i][j]=2735+j*10;
		}
	}
    // 假设温度矩阵已经初始化，这里省略初始化过程    
	FILE *outfile = fopen("thermal_image.jpg", "wb");
    if (outfile == NULL) {
        perror("Unable to create file");
        return 1;
    }

    // 初始化JPEG压缩对象
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, outfile);

    // 设置图像参数
    cinfo.image_width = WIDTH;
    cinfo.image_height = HEIGHT;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 90, TRUE);

    // 开始压缩
    jpeg_start_compress(&cinfo, TRUE);

    // 将温度矩阵转换为颜色，并写入JPEG文件
    JSAMPLE *row = malloc(3 * WIDTH * sizeof(JSAMPLE));
    for ( y = 0; y < HEIGHT; y++) {
        for ( x = 0; x < WIDTH; x++) {
            RGBColor color = temperature_to_color(temperature[y][x]);
            row[x * 3] = color.r;
            row[x * 3 + 1] = color.g;
            row[x * 3 + 2] = color.b;
        }
        jpeg_write_scanlines(&cinfo, &row, 1);
    }

    // 压缩结束
    jpeg_finish_compress(&cinfo);

    // 释放资源
    fclose(outfile);
    jpeg_destroy_compress(&cinfo);
    free(row);

#else
	int x, y;
#if 0
    int temperature[WIDTH][HEIGHT];
	for (y=0;y<WIDTH;y++) {
		for (x=0;x<HEIGHT;x++) {
			temperature[x][y]=2735+x*10;
		}
	}
#else
    int temperature[HEIGHT][WIDTH];
	for (y=0;y<HEIGHT;y++) {
		for (x=0;x<WIDTH;x++) {
			temperature[y][x]=2735+x*10;
		}
	}
#endif 
  // 假设温度矩阵已经初始化，这里省略初始化过程    // 创建一个PPM图像文件并写入数据
    FILE *fp = fopen("./thermal_image.ppm", "wb");
    if (fp == NULL) {
        perror("Unable to create file");
        return 1;
    }

    // 写入PPM文件的头信息
    fprintf(fp, "P6\n");
    fprintf(fp, "%d %d\n", WIDTH, HEIGHT);
    //fprintf(fp, "%d %d\n", 62 ,80);
    fprintf(fp, "255\n");

    // 将温度矩阵转换为颜色，并写入PPM文件
    for (y = 0; y < HEIGHT; y++) {
//	printf("write y=%d : ", y);
        for (x = 0; x < WIDTH; x++) {
		//printf (" %d,%d", x,y);
            RGBColor color = temperature_to_color(temperature[y][x]);
            fputc(color.r, fp); // 写入红色分量
            fputc(color.g, fp); // 写入绿色分量
            fputc(color.b, fp); // 写入蓝色分量
        }
//	printf("\n");
    }

    // 关闭文件
    fclose(fp);
#endif
    return 0;
}

