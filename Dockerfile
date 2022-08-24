FROM node:16.14.0-alpine3.15

# install dependencies
RUN apk update
RUN apk add build-base cmake gtk+2.0 curl libavc1394-dev linux-headers python3 findutils make g++ postgresql-dev postgresql-client libc6-compat
RUN ln -sf python3 /usr/bin/python

# install opencv 3.10
RUN mkdir /tmp-opencv && cd /tmp-opencv && \
  curl -L https://github.com/opencv/opencv/archive/3.1.0.tar.gz | tar zx && \
  cd /tmp-opencv/opencv-3.1.0 && mkdir build && cd build && \
  cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local -DENABLE_PRECOMPILED_HEADERS=OFF .. && \
  make -j7 && \
  make install && \
  cd /tmp-opencv/opencv-3.1.0/build/lib && \
  mv * /usr/local/lib && \
  rm -r /tmp-opencv

# Install server dependencies
WORKDIR /home/node/app/server
COPY src/server/package*.json ./
RUN npm install

# Get app sources
WORKDIR /home/node/app
COPY src .

# Build solver executable
WORKDIR /home/node/app/solver
RUN cmake . && make

# Expose ports
EXPOSE 3000

# Run server
WORKDIR /home/node/app/server
CMD ["npm", "run", "dev"]
