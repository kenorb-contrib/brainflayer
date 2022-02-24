# our base image
FROM ubuntu18.04-for-brainflayer

# Set Term
ENV TERM linux \
        DEBIAN_FRONTEND noninteractive \
CMD ["/bin/bash"]

# Create New User - Vermil
RUN useradd -rm --password Password123 -d /home/brainflayer -s /bin/bash -g root -G sudo -u 1000 brainflayer
USER brainflayer
WORKDIR /home/brainflayer

# Clone My Git Repository, Autoconfig, Make, Install, Add SSH, Cleanup
RUN cd /home/brainflayer \
        && git clone https://github.com/mpossengineer/brainflayer \
        && cd ./brainflayer \
        && make all
~                         
