This project provides a simple C++ program that reads a YAML file that describes the episodes and seasons of a collection of DVD images, and uses HandBrakeCLI to encode the episodes into a collection of video files, according to the configuration options provided.

The program uses linux extended filesystem attributes to record the MD5 sum of the DVD images with the original content, so that re-running the program on the same configuration files will re-encode any videos who's DVD has changed.

Additionally, the outputted video files also have a checksum of the commandline given to HandBrakeCLI, so that re-running the program with different configuraiton options will replace the video files who's options have changed.

The intended usage of this program is to convert, ahead of time, my personal DVD collection into files that can be sent to my Chromecast, using Plex. This allows my Plex server to send the video files to the chromecast with zero transcoding.

