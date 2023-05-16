/*
Bryan Pham
This Edge Detection program is an image processing technique used to find discontinuities in digital images. 
Create an image edge detector that adheres to the following requirements. The program will be written in C. 
It will take one or more P6 images as input and apply a Laplacian filter to the images using threads. For each 
input image, the program will create a new P6 image as output that has the edges of the input image.  
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>

#define LAPLACIAN_THREADS 23     //change the number of threads as you run your concurrency experiment

/* Laplacian filter is 3 by 3 */
#define FILTER_WIDTH 3       
#define FILTER_HEIGHT 3      

#define RGB_COMPONENT_COLOR 255

typedef struct {
      unsigned char r, g, b;
} PPMPixel;

struct parameter {
    PPMPixel *image;         //original image pixel data
    PPMPixel *result;        //filtered image pixel data
    unsigned long int w;     //width of image
    unsigned long int h;     //height of image
    unsigned long int start; //starting point of work
    unsigned long int size;  //equal share of work (almost equal if odd)
};


struct file_name_args {
    char *input_file_name;      //e.g., file1.ppm
    char output_file_name[20];  //will take the form laplaciani.ppm, e.g., laplacian1.ppm
};

double total_elapsed_time = 0;

pthread_mutex_t mutex_a = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_b = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t mutex_c = PTHREAD_MUTEX_INITIALIZER;

/*This is the thread function. It will compute the new values for the region of image specified in params (start to start+size) using convolution.
    For each pixel in the input image, the filter is conceptually placed on top of the image with its origin lying on that pixel.
    The  values  of  each  input  image  pixel  under  the  mask  are  multiplied  by the corresponding filter values.
    Truncate values smaller than zero to zero and larger than 255 to 255.
    The results are summed together to yield a single output value that is placed in the output image at the location of the pixel being processed on the input.
 
 */
void *compute_laplacian_threadfn(void *params)
{
    int laplacian[FILTER_WIDTH][FILTER_HEIGHT] =
    {
        {-1, -1, -1},
        {-1,  8, -1},
        {-1, -1, -1}
    };

    int red, green, blue;

    struct parameter *param = (struct parameter *) params;

    int x_coordinate, y_coordinate = 0;
    //The for-loop goes to each pixel and applying filter.
    for(int iteratorImageWidth = 0; iteratorImageWidth < param->w; iteratorImageWidth++)
    {
        for(int iteratorImageHeight = param->start; iteratorImageHeight < param->start + param->size; iteratorImageHeight++)
        {
            red = 0;
            green = 0;
            blue = 0;
            for(int iteratorFilterWidth = 0; iteratorFilterWidth < FILTER_WIDTH; iteratorFilterWidth++)
            {
                for(int iteratorFilterHeight = 0; iteratorFilterHeight < FILTER_HEIGHT; iteratorFilterHeight++)
                {
                    x_coordinate = ( iteratorImageWidth - FILTER_WIDTH / 2 + iteratorFilterWidth + param->w ) % param->w;
                    y_coordinate = ( iteratorImageHeight - FILTER_HEIGHT / 2 + iteratorFilterHeight + param->h ) % param->h;
                    red+= param->image[y_coordinate * param->w + x_coordinate].r * laplacian[iteratorFilterHeight][iteratorFilterWidth];
                    green+= param->image[y_coordinate * param->w + x_coordinate].g * laplacian[iteratorFilterHeight][iteratorFilterWidth];
                    blue+= param->image[y_coordinate * param->w + x_coordinate].b * laplacian[iteratorFilterHeight][iteratorFilterWidth];
                }
            }

            //Truncate values smaller than zero to zero and larger than 255 to 255.
            if(red < 0) red = 0;
            else if(red > 255) red = 255;
            if(green < 0) green = 0;
            else if(green > 255) green = 255;
            if(blue < 0) blue = 0;
            else if(blue > 255) blue = 255;

            //Adding the rgb color to results.
            param->result[iteratorImageHeight * param->w + iteratorImageWidth].r = red;  
            param->result[iteratorImageHeight * param->w + iteratorImageWidth].g = green; 
            param->result[iteratorImageHeight * param->w + iteratorImageWidth].b = blue;
        }
    }
    return NULL;
}

/* Apply the Laplacian filter to an image using threads.
 Each thread shall do an equal share of the work, i.e. work=height/number of threads. If the size is not even, the last thread shall take the rest of the work.
 Compute the elapsed time and store it in *elapsedTime (Read about gettimeofday).
 Return: result (filtered image)
 */
PPMPixel *apply_filters(PPMPixel *image, unsigned long w, unsigned long h, double *elapsedTime) 
{
    struct timeval start, end;
    gettimeofday(&start, NULL);

    PPMPixel *result = (PPMPixel*)malloc(w * h * sizeof(PPMPixel));
    struct parameter params[LAPLACIAN_THREADS];
    int work = h / LAPLACIAN_THREADS;
    pthread_t t[LAPLACIAN_THREADS];

    for(int i = 0; i < LAPLACIAN_THREADS; i++)
    {
        params[i].image = image;
        params[i].result = result;
        params[i].start = i * work;
        params[i].w = w;
        params[i].h = h;

        //Making sure that the last thread take on the rest of the work
        if(i == LAPLACIAN_THREADS -1)
        {
            params[i].size = params[i].h - params[i].start;
        }
        else
        {
            params[i].size = work;
        }

        pthread_mutex_lock(&mutex_a);
        if(pthread_create(&t[i], NULL, compute_laplacian_threadfn, (void*)&params[i]) != 0)
        {
            fprintf(stderr, "Unable to create thread %d\n", i);
        }
        pthread_mutex_unlock(&mutex_a);
       
    }

    for(int i = 0; i < LAPLACIAN_THREADS; i++)
    {
        pthread_join(t[i], NULL);
    }

    gettimeofday(&end, NULL);
    pthread_mutex_lock(&mutex_c);
    //Get the total time of threadings
    *elapsedTime += (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_usec - start.tv_usec) / 1000000.0;
    pthread_mutex_unlock(&mutex_c);
    return result;
}

/*Create a new P6 file to save the filtered image in. Write the header block
 e.g. P6
      Width Height
      Max color value
 then write the image data.
 The name of the new file shall be "filename" (the second argument).
 */
void write_image(PPMPixel *image, char *filename, unsigned long int width, unsigned long int height)
{
    //Openning file to write btyes
    FILE *fp = fopen(filename, "wb");
    if(!fp)
    {
        fprintf(stderr, "Unable to open file '%s'\n", filename);
    }
    //Writing the header block
    fprintf(fp, "P6\n");
    fprintf(fp, "%lu %lu\n", width, height);
    fprintf(fp, "%d\n", RGB_COMPONENT_COLOR);
    fwrite(image, 3 * width, height, fp);

    fclose(fp); 
}

/* Open the filename image for reading, and parse it.
    Example of a ppm header:    //http://netpbm.sourceforge.net/doc/ppm.html
    P6                  -- image format
    # comment           -- comment lines begin with
    ## another comment  -- any number of comment lines
    200 300             -- image width & height 
    255                 -- max color value
 
 Check if the image format is P6. If not, print invalid format error message.
 If there are comments in the file, skip them. You may assume that comments exist only in the header block.
 Read the image size information and store them in width and height.
 Check the rgb component, if not 255, display error message.
 Return: pointer to PPMPixel that has the pixel data of the input image (filename).The pixel data is stored in scanline order from left to right (up to bottom) in 3-byte chunks (r g b values for each pixel) encoded as binary numbers.
 */
PPMPixel *read_image(const char *filename, unsigned long int *width, unsigned long int *height)
{
    PPMPixel *img;
    char buff[sizeof(char *) * 8];
    int rgb_component;
    unsigned char rgb_color[3];

    //Opening file
    FILE *fp = fopen(filename, "rb");
    if(!fp) 
    {
        fprintf(stderr, "Unable to open file '%s'\n", filename);
        pthread_exit(0);
    }
    
    //Checking if the image format is 'P6'
    if(!fgets(buff, sizeof(buff), fp) || (buff[0] != 'P' || buff[1] != '6'))
    {
        fprintf(stderr, "Invalid image format error must be 'P6'\n");
        pthread_exit(0);
    }

    //Skipping all the comments in the file
    int x = getc(fp);
    while (x == '#')
    {
        while (getc(fp) != '\n');
        x = getc(fp);
    }
    ungetc(x, fp);

    //Reading the image size information and store them in width and height
    if(fscanf(fp, "%ld %ld", width, height) != 2)
    {
        fprintf(stderr, "Invalid image size error '%s'\n", filename);
        pthread_exit(0);
    }

    //Checking if rgb_component has 255 components
    if(fscanf(fp, "%d", &rgb_component) != 1 || rgb_component != RGB_COMPONENT_COLOR)
    {
        fprintf(stderr, "'%s' is not have a valid 255 components\n", filename);
        pthread_exit(0);
    }
    //Skipping a line in the file to get to all the rgb colors.
    while(fgetc(fp) != '\n');

    //Getting the whole ppm image rgb color and put it into img variable.
    img = (PPMPixel*)malloc(*width * *height * sizeof(PPMPixel));
    for(int i = 0; i < *width * *height; i++)
    {
        //Read in the rgb color from the image to rgb_color 
        if(fread(rgb_color, 3, 1, fp) < 1)
        {
            break;
        }
        img[i].r = rgb_color[0];
        img[i].g = rgb_color[1];
        img[i].b = rgb_color[2];
    }

    fclose(fp);
    return img;
}

/* The thread function that manages an image file. 
 Read an image file that is passed as an argument at runtime. 
 Apply the Laplacian filter. 
 Save the result image in a file called laplaciani.ppm, where i is the image file order in the passed arguments.
 Example: the result image of the file passed third during the input shall be called "laplacian3.ppm".
*/
void *manage_image_file(void *args)
{
    //Initializing file_name_args
    struct file_name_args* file_name = (struct file_name_args*) args;
    unsigned long int width;
    unsigned long int height;

    PPMPixel *img = read_image(file_name->input_file_name, &width, &height);

    PPMPixel *result = apply_filters(img, width, height, &total_elapsed_time);

    if(result)
    {
        write_image(result, file_name->output_file_name, width, height);
    }
    free(result);

    free(img);
    return NULL;
}
/*The driver of the program. Check for the correct number of arguments. If wrong print the message: "Usage ./a.out filename[s]"
  It shall accept n filenames as arguments, separated by whitespace, e.g., ./a.out file1.ppm file2.ppm    file3.ppm
  It will create a thread for each input file to manage.  
  It will print the total elapsed time in .4 precision seconds(e.g., 0.1234 s). 
  The total elapsed time is the total time taken by all threads to compute the edge detection of all input images .
 */
int main(int argc, char *argv[])
{
    if(argc <= 1)
    {
        fprintf(stderr, "Usage: %s filename[s]\n", argv[0]);
        return 0;
    }

    argc--;
    argv++;

    pthread_t t[argc];
    struct file_name_args *file_name = calloc(argc, sizeof(struct file_name_args));
    for(int i = 0; i < argc; i++) 
    {
        file_name[i].input_file_name = argv[i];

        //If there is result then it will write a file called laplaciani.ppm where i is the image file order in the passed arguments.
        pthread_mutex_trylock(&mutex_b);
        snprintf(file_name[i].output_file_name, 20, "laplacian%d.ppm", i +1);
        pthread_mutex_unlock(&mutex_b);

        if(pthread_create(&t[i], NULL, manage_image_file, (void*)&file_name[i]) != 0)
        {
            fprintf(stderr, "Unable to create thread %d!\n", i);
        }
    }
    for(int i = 0; i < argc; i++)
    {
        pthread_join(t[i], NULL);
    }
    free(file_name);
    printf("Time: %.4f\n", total_elapsed_time);
    return 0;
}

