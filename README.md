# MDFourier

A Fourier Transform analysis tool to compare different Sega Genesis/Mega Drive audio hardware revisions. Other consoles will probably follow.

This is a set of tools intended to compare Mega Drive/Sega Genesis audio characteristics aided by Fourier analysis. 

# Objective

Provide a Fourier based analysis framework to compare audio generated by any MegaDrive/Genesis variant, clone or implementation. This is of course not limited to the Mega Drive, and the software with minor changes can be used to compare any other platform.
    
With these tools any Sega Genesis audio implementation or hardware variant can be evaluated against the frequency response of any other, in order to compare how similar they are. As a result they could also be fine tuned to closely resemble the desired result, since any reference audio file generated with the 240p Test Suite can be used to compare it to any other.

The intention is not to disparage any particular implementation, but help to understand and improve them whenever possible by identifying the differences. This can help emulators, FPGA implementations and even hardware modifications to better match the desired reference recording.

Although I believe any present and future implementation should offer a configuration based on vintage retail hardware, that doesn't mean there isn't room for improvement. Reducing noise while keeping the original sound signature is one such area.

The YM2612 operates just like the YM3812 does, in that it has an internal log transformed table. Modern implementations like MAME, the MAME derived core in Genesis Plus GX, Nuked, BlastEm and Exodus should all have bit perfect recreations of the tables used in the original chip. However there are differences that one could argue must come from the analog side. This work intends to offer the data analysis tools to help in that regard.

I believe that it can be done with higher accuracy, we should pursue it. So this work is intended to help reach that goal. Fans can compare variants, and people dedicated to replication, preservation and mods could fine tune according to their intentions.

I must emphasize I am not an expert in any of the related topics, just interested in preservation and FM sound. Any corrections and improvements are encouraged and welcome.

# Dependencies

FFTW library:   http://www.fftw.org/

Regularized Incomplete Beta Function: https://github.com/codeplea/incbeta

