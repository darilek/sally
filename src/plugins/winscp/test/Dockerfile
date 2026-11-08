FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends openssh-server && rm -rf /var/lib/apt/lists/* \
 && mkdir -p /run/sshd \
 && useradd -m -s /bin/bash test && echo 'test:test' | chpasswd \
 && printf 'PasswordAuthentication yes\nHostKeyAlgorithms +ssh-rsa\nPubkeyAcceptedAlgorithms +ssh-rsa\nKexAlgorithms +diffie-hellman-group-exchange-sha256,diffie-hellman-group14-sha1\n' > /etc/ssh/sshd_config.d/winscp-test.conf \
 && mkdir -p /home/test/test && dd if=/dev/urandom of=/home/test/test/big.bin bs=1M count=50 \
 && chown -R test:test /home/test
EXPOSE 22
CMD ["/usr/sbin/sshd", "-D", "-e"]