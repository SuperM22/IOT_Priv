# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/Users/mimmo/esp/esp-idf/components/bootloader/subproject"
  "/Users/mimmo/UniProjects/IOT_Priv/fft/build/bootloader"
  "/Users/mimmo/UniProjects/IOT_Priv/fft/build/bootloader-prefix"
  "/Users/mimmo/UniProjects/IOT_Priv/fft/build/bootloader-prefix/tmp"
  "/Users/mimmo/UniProjects/IOT_Priv/fft/build/bootloader-prefix/src/bootloader-stamp"
  "/Users/mimmo/UniProjects/IOT_Priv/fft/build/bootloader-prefix/src"
  "/Users/mimmo/UniProjects/IOT_Priv/fft/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/mimmo/UniProjects/IOT_Priv/fft/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/mimmo/UniProjects/IOT_Priv/fft/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
