/***************************************************************************
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************/
#version 420
#include "HlslGlslCommon.h"

CONSTANT_BUFFER(PerImageCB, 0)
{
    sampler2D gTexture;
};
 
vec4 calcColor(vec2 texC)
{
    const float blurStrength = 2.7; // Controls the strength of the blur effect. Higher values will make it more noticable at the center of the image
    const float offsets[5] = {0.005,0.01,0.03,0.05,0.075}; // Controls the shape of the blur
 
    // get the direction we are going to sample from
    vec2 sampleDirection = 0.5 - texC; 
    float dirLength = length(sampleDirection);
    sampleDirection = sampleDirection/dirLength;
 
    // Get the original color
    vec4 texelColor = texture(gTexture, texC);  
    vec4 blurColor = texelColor;
 
    // sample based on the direction
    for (int i = 0; i < 5; i++)
    {
      blurColor += texture( gTexture, texC + sampleDirection * offsets[i] );
      blurColor += texture( gTexture, texC + sampleDirection * -offsets[i] );
    }
    blurColor *= 1.0/11;
 
    // Blend the results
    float weight = dirLength * blurStrength;
    weight = clamp(weight,0.0,1.0);
    vec4 fragColor = mix(texelColor, blurColor, weight);

    return fragColor;
} 

 #ifdef FALCOR_HLSL
 float4 main(in vec2 texC : TEXCOORD) : SV_TARGET
 {
    return calcColor(texC);
 }
 #elif defined FALCOR_GLSL
in vec2 texC;
out vec4 fragColor;

void main()
{
    fragColor = calcColor(texC);
}
 #endif
