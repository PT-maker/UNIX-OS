FROM ubuntu:24.04
COPY run.sh /run.sh
RUN sed -i 's/\r$//' /run.sh && chmod +x /run.sh
VOLUME /data
CMD ["/run.sh"]