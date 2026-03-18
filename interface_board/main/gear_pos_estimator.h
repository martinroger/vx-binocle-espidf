#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <algorithm>

#include "global_vars.hpp"

class MedianFilter5 {
private:
    float buffer[5] = {0};
    int insertIndex = 0;
    bool isFull = false;

public:
    float update(float newValue) {
        // Add new value to circular buffer
        buffer[insertIndex] = newValue;
        insertIndex = (insertIndex + 1) % 5;
        if (insertIndex == 0) isFull = true;

        // Create a copy to sort so we don't ruin the temporal order of the buffer
        float sortBuffer[5];
        // for (int i = 0; i < 5; i++) {
        //     sortBuffer[i] = buffer[i];
        // }
        memcpy(sortBuffer,buffer,sizeof(sortBuffer));

        // Sort the copy (std::sort is very fast for 5 elements)
        std::sort(sortBuffer, sortBuffer + 5);

        // Return the middle element
        return sortBuffer[2];
    }

    // Call this in setup() to prevent the filter starting from 0
    void seed(float value) {
        for (int i = 0; i < 5; i++) buffer[i] = value;
        isFull = true;
    }
};