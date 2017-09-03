This repo provides an example for image detection and replacement with C++.

It builds two mechanism internally for image detection: using openCV library and manually calculating the mean square error based on RGB colour.
* For approach using openCV, a template image is used.
* For the manual approach, a predefined square with filled colour is provided in the code as a template

The source provides a command line interface with usage like:
Usage: imageDetect -r path/to/replacement/image [-t path/to/template/iamge] [-o path/to/output/image] path/to/target/image
	* -h prints this help
	* Size of template and replacement images must be identical
	* Without output path specified, output.ppm will be generated in the current foler
	* Without path to template image, you choose to use default one: a red square with side length 50pixel
  
  To build, cd into build folder and do:
  cmake ../src
  make
  
  It would need openCV installed in the system for compiling.
