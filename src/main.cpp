#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <iostream>
#include <iterator>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

using std::cout;
using std::endl;

// For RGB color, 3byte is used to describe color of a pixel.
#define RGB_NUM_COLOR 3U

static bool read_line(FILE *infile, char *s)
{
  char *news, *sptr;
  static char line[80]="";

  if ((strlen(line) == 0) || ((sptr = strtok (NULL, " \n\t")) == NULL))
  {
     while ((news = fgets (line, 80, infile)) != NULL)
        if ( (line[0] != '#') && ((sptr = strtok (line, " \n\t")) != NULL) )
           break;

     if (news == 0)
        return false;
  }

  strcpy(s, sptr);
  return true;
}

static bool read_ppm_image(const char * fileName, std::vector<unsigned char>& image, int& width, int& height) {
   FILE* infile = fopen(fileName, "rb");
   if ( infile == NULL )
      return false;

   char s[1024]="";

   fgets(s, 4, infile);
   if(strcmp(s, "P6\n" ) != 0)
      return false;

   bool result=true;
   result = (result && read_line(infile, s)); width  = atoi(s);
   result = (result && read_line(infile, s)); height = atoi(s);
   result = (result && read_line(infile, s)); /*maxval = atoi(s);*/
   if(!result) return 0;

   image.resize(width*height*RGB_NUM_COLOR);
   if(fread(&image[0], 3, height*width, infile ) != (size_t)(height*width))
      return false;
   

   fclose(infile);
   return true;
}

static bool write_ppm_image(const char* fileName, std::vector<unsigned char> const& image, int width, int height)
{
   FILE *outfile = fopen ( fileName, "wb" );
   if(outfile == 0)
      return false;

   fprintf(outfile, "P6\n#Constructed by Mirriad\n%d %d\n255\n", width, height);
   fwrite(&image[0], RGB_NUM_COLOR, height*width, outfile);
   fclose(outfile);
   return true;
}

#define sq(x) ((x)*(x))
#define sqaure_sum(a1,a2,a3,b1,b2,b3) \
    sqrt(sq(a1-b1)+sq(a2-b2)+sq(a3-b3))

/**
  * @brief Base class for shapes.
**/
class shape_base {};

/**
  * @brief A rectangle class. 
  *   It must have the size of two dimentions. 
  *   The coordinates of the left top vertice and filled colour is optional.
**/
class rectangular: public shape_base
{
public:
    rectangular(int len_cols, int len_rows, int left_corner_x=0, int left_corner_y=0): x(left_corner_x), y(left_corner_y), cols(len_cols), rows(len_rows) {}
    void fillRGB(int R, int G, int B)
    {
        red_fill = R;
        green_fill = G;
        blue_fill = B; 
    }
    int x;
    int y;
    int cols;
    int rows;
    int red_fill;
    int green_fill;
    int blue_fill;
};

/**
  * @brief Base class for image detectors.
**/
class image_detector_base
{
public:
    virtual rectangular* detect(std::string target_image_path) {}
};

/**
  * @brief This class utilises algrithm called similarity to detect whether a match of
  *   a given shape exists in a target image. The algrithm is straight forward: it calculates
  *   root mean square error of the color of all pixels in the target image to the given template shape.
  *   If there is a perfect match, i.e. an identcal shape, presented in the target image, the root mean
  *   square error of that area will be 0. For any other area of the target image, the root mean square
  *   error will be a positive value in (0, 255]. The colour code here uses RGB. The same 
  *   idea applies to other colour standards.
**/
class image_detector_similarity: public image_detector_base
{
public:
    image_detector_similarity(rectangular& template_shape): tmplt_shape(template_shape) {}
    rectangular* detect(const std::string target_image_path)
    {
        
        rectangular *p_match = 0;
        std::pair<int, int> point;
        std::vector<int> matches;

        (void)read_ppm_image(target_image_path.c_str(), content, target_width, target_height);

        // Iterate through all pixels in the target image with assumption that the match to 
        // the template does not cross image boundaries.
        for (int i = 0; i < target_height-tmplt_shape.cols; ++i)
        {
            for (int j = 0; j < target_width-tmplt_shape.rows; ++j)
            {
                point.first = j;
                point.second = i;
                // Build a vector with results how matching an area in the target image to the
                // template. The area is defined as a rectangle same size with the template
                // starting from its left top vertice.
                matches.push_back(match_norm(point));
            }
        }

        // Find out the min of the matching results and where it is.
        auto smallest = std::min_element(matches.begin(), matches.end());
        auto offset = std::distance(matches.begin(), smallest);
        std::cout << "min element is " << *smallest << " at position " << offset << std::endl;  

        // Find out the pixal coordinates which leads the matching area.
        int y = offset / (target_width-tmplt_shape.rows);
        int x = offset % (target_width-tmplt_shape.rows);
        cout << "coordinate: " << x << " " << y << endl;

        // Return the wrapped matching area as a rectangle.
        p_match = new rectangular(tmplt_shape.rows, tmplt_shape.cols, x, y);
        return p_match;
    }
private:
    int match_norm(std::pair<int,int>& point)
    {
        int match_merit = 0;
        // Calculate the start index in the RGB array for the given pixel.
        auto offset = content.begin() + (point.second * target_width + point.first) * RGB_NUM_COLOR;

        // Scan all pixels in the given area, calculate the root mean square error 
        for (int i = 0; i < tmplt_shape.rows; ++i)
        {
            auto start = offset + target_width*RGB_NUM_COLOR*i;
            for (auto iter = start; iter != start+tmplt_shape.cols*RGB_NUM_COLOR; iter+=RGB_NUM_COLOR)
            {
                int v = sqaure_sum(int(*iter), int(*(iter+1)), int(*(iter+2)), 255, 0, 0);
                match_merit += v;
            }
        }
        match_merit = sqrt(match_merit/tmplt_shape.cols/tmplt_shape.rows);
        return match_merit;
    }

    rectangular& tmplt_shape;
    std::vector<unsigned char> content;
    int target_width, target_height;
};

/**
  * @brief This class ustilise the buildin method in open source library OpenCV for image
  *   detection. Multiple algrithms are available: CV_TM_SQDIFF, CV_TM_CCORR, CV_TM_CCOEFF
  *   and their normalised version. 
**/
class image_detector_CV: public image_detector_base
{
public:
    image_detector_CV(const std::string &template_image_path = "", int method=0): tmplt_path(template_image_path), match_method(method)
    {
        templt = cv::imread(template_image_path, CV_LOAD_IMAGE_COLOR);
    }

    rectangular* detect(std::string target_image_path)
    {
        cv::Mat img, match_result;
        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::Point matchLoc;
        rectangular* p_match = 0;
        int result_cols, result_rows;

        img = cv::imread(target_image_path, CV_LOAD_IMAGE_COLOR);
        result_cols =  img.cols - templt.cols + 1;
        result_rows = img.rows - templt.rows + 1;
        match_result.create(result_rows, result_cols, CV_32FC1);

        cv::matchTemplate(img, templt, match_result, match_method);
        cv::normalize(match_result, match_result, 0, 1, cv::NORM_MINMAX, -1, cv::Mat());

        cv::minMaxLoc(match_result, &minVal, &maxVal, &minLoc, &maxLoc, cv::Mat());

        if (minVal != maxVal)
        {
            // For CV_TM_SQDIFF, the min value is the matching point. It is different for other algrithms.
            matchLoc = minLoc;
            p_match = new rectangular(matchLoc.x, matchLoc.y, templt.rows, templt.cols);
        }

        return p_match;
    }

private:
    const std::string& tmplt_path;
    cv::Mat templt;
    int match_method;
};

/**
  * @brief Base class for image render. So far it only does one thing, put an overlay image on top
  *   of source image at given location.
**/
class image_render_base
{
public:
    image_render_base(std::string& image_path): path_image_in(image_path) {}
    virtual image_render_base& set_overlay_rect(rectangular& rect, std::string& overlay_path) {}
    virtual void to(std::string& image_path) {}
protected:
    std::string path_image_in;
    std::string path_image_out;
};

/**
  * @brief The render of similarity algrithm version. 
**/
class image_render_similarity: public image_render_base
{
public:
    image_render_similarity(std::string& image_path): image_render_base(image_path)
    {
        
        (void)read_ppm_image(image_path.c_str(), content, width, height);
    }

    image_render_base& set_overlay_rect(rectangular& rect, std::string& overlay_path)
    {
        std::vector<unsigned char> overlay_content;
        int overlay_width, overlay_height;

        (void)read_ppm_image(overlay_path.c_str(), overlay_content, overlay_width, overlay_height);

        std::pair<int, int> point;

        // Line by line, copy the color value of each pixel of the overlay image to the source image.
        for (int i = 0; i < overlay_height; ++i)
        {
            int overlay_offset = overlay_width * i * RGB_NUM_COLOR;
            int content_offset = ((rect.y + i ) * width + rect.x) * RGB_NUM_COLOR;
            std::copy(overlay_content.begin()+overlay_offset, overlay_content.begin()+overlay_offset+overlay_width*RGB_NUM_COLOR, content.begin()+content_offset);
        }

        return *this;
    }

    void to(std::string& image_path)
    {
        (void)write_ppm_image(image_path.c_str(), content, width, height);
    }
private:
    std::vector<unsigned char> content;
    int width, height;
};

/**
  * @brief The render of OpenCV version. OpenCV library uses buildin methods for applying overlay and exporting.
**/
class image_render_CV: public image_render_base
{
public:
    image_render_CV(std::string& image_path): image_render_base(image_path)
    {
        img = cv::imread(image_path, CV_LOAD_IMAGE_COLOR);
    }

    image_render_base& set_overlay_rect(rectangular& rect, std::string& overlay_path)
    {
        cv::Mat img_overlay = cv::imread(overlay_path, CV_LOAD_IMAGE_COLOR);

        img_overlay.copyTo(img(cv::Rect(rect.cols,rect.rows, rect.x,rect.y)));
        return *this;
    }

    void to(std::string& image_path)
    {
        cv::imwrite(image_path, img);
    }
private:
    cv::Mat img;
};

// extern char *optarg;
static void display_help(void)
{
    cout << "Usage: imageDetect -r path/to/replacement/image [-t path/to/template/iamge] [-o path/to/output/image] path/to/target/image" << endl;
    cout << "\t* -h prints this help" << endl;
    cout << "\t* Size of template and replacement images must be identical" << endl;
    cout << "\t* Without output path specified, output.ppm will be generated in the current foler" << endl;
    cout << "\t* Without path to template image, you choose to use default one: a red square with side length 50pixel" << endl;

}
/** @function main */
int main( int argc, char** argv )
{
    const char *optString = "o:t:r:h";
    rectangular *p_match = 0;
    std::string overlay_path, tmplt_path, target_path;
    std::string output_path("./output.ppm"); // Default path of the output image.
    int opt;

    if (argc < 2)
    {
        cout << "Too few arguments." << endl;
        exit(-1);
    }

    // Parse arguments from command line.
    while ((opt = getopt( argc, argv, optString))!= -1)
    {
        switch (opt)
        {
            case 'o':
                output_path = std::string(optarg);
                break;
            case 't':
                tmplt_path = std::string(optarg);
                break;
            case 'r':
                overlay_path = std::string(optarg);
                break;
            case 'h':
                display_help();
                exit(0);
            default:
                cout << "Invalid!" << endl;
                break;  
        }
    }

    if (optind >= argc) {
        cout << "error" << endl;
    }

    // This is the path to image we run detection against.
    target_path = argv[optind];

    if (overlay_path.empty() || target_path.empty())
    {
        cout << "Argument error" << endl;
        exit(-1);
    }

    image_detector_base *p_detector = 0;
    image_render_base *p_render = 0;

    // Without presence of external template image, initialize 'similarity' algrithm with
    // coloured shape as a template.
    if (tmplt_path.empty())
    {
        // Here we generate a retanglar in red as template.
        int col = 50;
        int row = 50;
        rectangular rec(col, row);
        rec.fillRGB(255, 0, 0);

        p_detector = new image_detector_similarity(rec);
        p_render = new image_render_similarity(target_path);
    }
    // Alternately, initialize CV_TM_SQDIFF algrithm with provided template image.
    else
    {
        p_detector = new image_detector_CV(tmplt_path, CV_TM_SQDIFF);
        p_render = new image_render_CV(target_path);
    }

    // Run the associated detection algrithm. If a match is located, replace it with
    // given overlay image before exporting the result as a new image.
    p_match = p_detector->detect(target_path);
    if (0 != p_match)
    {
        p_render->set_overlay_rect(*p_match, overlay_path).to(output_path);
    }

    return 0;
}