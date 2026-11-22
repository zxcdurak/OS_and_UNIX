FROM alpine:3.22

ENV DATA_DIR=/data

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

VOLUME ["/data"]

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]