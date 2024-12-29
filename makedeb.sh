#!/bin/bash

# Exit on any error
set -e

echo "Building WTF Package..."

# Clean previous builds
rm -f wtf_package/usr/bin/*
rm -f wtf_package/.wtf/res/*

# Create necessary directories if they don't exist
mkdir -p wtf_package/usr/bin
mkdir -p wtf_package/.wtf/res

# Build both architectures
echo "Building AMD64 binary..."
make amd64
echo "Building i386 binary..."
make i386

# Copy binaries
echo "Copying binaries..."
cp build/wtf_amd64 wtf_package/usr/bin/
cp build/wtf_i386 wtf_package/usr/bin/
chmod +x wtf_package/usr/bin/wtf_*

# Copy definitions file
echo "Copying definitions file..."
cp .wtf/res/definitions.txt wtf_package/.wtf/res/

# Set correct permissions
chmod 755 wtf_package/DEBIAN/postinst
chmod 755 wtf_package/DEBIAN/prerm

# Build the package
echo "Building Debian package..."
dpkg-deb --build wtf_package

# Rename package to include version and architecture
VERSION=$(grep Version wtf_package/DEBIAN/control | cut -d' ' -f2)
mv wtf_package.deb wtf_${VERSION}_x86_amd64.deb

echo "Package built successfully: wtf_${VERSION}_x86_amd64.deb"