#!/bin/bash

# Set up paths
IOS_SDK_VERSION=$(xcrun --sdk iphoneos --show-sdk-version)
SIMULATOR_SDK_PATH=$(xcrun --sdk iphonesimulator --show-sdk-path)
IOS_SDK_PATH=$(xcrun --sdk iphoneos --show-sdk-path)

# Minimum iOS version for deployment
MIN_IOS_VERSION=12.0

# Build for iOS (arm64)
echo "Building for iOS (arm64)..."
./configure CFLAGS="-arch arm64 -pipe -std=c99 -isysroot $IOS_SDK_PATH -miphoneos-version-min=$MIN_IOS_VERSION" \
            --host=arm-apple-darwin \
            --enable-static \
            --disable-frontend \
            --disable-debug \
            --disable-dependency-tracking

make clean
make

#echo "Store the result for the iOS build"
#cd libmp3lame/.libs
#echo "Navigate to libs"
#mv libmp3lame.a libmp3lame-ios.a
#ls
#cd ../..

# Build for iOS Simulator (x86_64 and arm64 for Apple Silicon Macs)
#echo "Building for iOS Simulator (x86_64 and arm64)..."
#./configure CFLAGS="-arch x86_64 -pipe -std=c99 -isysroot $SIMULATOR_SDK_PATH -mios-simulator-version-min=$MIN_IOS_VERSION" \
#            --host=x86_64-apple-darwin \
#            --enable-static \
#            --disable-frontend \
#            --disable-debug \
#            --enable-expopt=full \
#            --disable-shared \
#            --disable-dependency-tracking
#
#make clean
#make
#
#echo "Store the result for the iOS Simulator (x86_64)"
#cd libmp3lame/.libs
#echo "Navigate to libs"
#mv libmp3lame.a libmp3lame-sim-x86_64.a
#ls
#cd ../..
#
## Build for iOS Simulator (arm64 for Apple Silicon Macs)
#./configure CFLAGS="-arch arm64 -pipe -std=c99 -isysroot $SIMULATOR_SDK_PATH -mios-simulator-version-min=$MIN_IOS_VERSION" \
#            --host=arm64-apple-darwin \
#            --enable-static \
#            --disable-frontend \
#            --disable-debug \
#            --enable-expopt=full \
#            --disable-shared \
#            --disable-dependency-tracking
#
#make clean
#make
#
#echo "Store the result for the iOS Simulator (arm64)"
#cd libmp3lame/.libs
#echo "Navigate to libs"
#mv libmp3lame.a libmp3lame-sim-arm64.a
#ls
#cd ../..

# Create a universal (fat) library for iOS and iOS Simulator (x86_64 + arm64)
#cd libmp3lame/.libs
#echo "Navigate to libs"
#ls
#echo "Creating a universal library..."
#lipo -create -output libmp3lame.a libmp3lame-ios.a libmp3lame-sim-x86_64.a libmp3lame-sim-arm64.a

# Clean up intermediate files
# rm libmp3lame-ios.a libmp3lame-sim-x86_64.a libmp3lame-sim-arm64.a

echo "Build complete. Universal library created as libmp3lame.a"
