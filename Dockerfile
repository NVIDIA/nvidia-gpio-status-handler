FROM ubuntu:18.04
LABEL Description="NVIDIA OOB Active Monitoring and Logging Development Environment"
LABEL Version="0.1"
LABEL Maintainer="Kun Zhao(kuzhao@nvidia.com)"
LABEL ImageName="bldenv:module"
ARG execdir=/usr/bin
ARG scrpt=setup_bldenv
ADD scripts/$scrpt $execdir
SHELL ["/bin/bash", "-c"]
RUN chmod +x $execdir/$scrpt
RUN $execdir/$scrpt
