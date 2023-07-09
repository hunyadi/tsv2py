FROM quay.io/pypa/manylinux_2_28_x86_64
COPY . package/
RUN cd package && scripts/dist.sh
