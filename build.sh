docker build . -t esp32
docker run --rm -it -v $(pwd):/usr/build -w /usr/build esp32 make
