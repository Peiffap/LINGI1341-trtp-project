int main(int argc, char *argv[]) {
	while (!disconnect) {
		receive_bitstream();
		interpret_bitstream();
		write_payload();
	}
}

void receive_bitstream() {

}

char *interpret_bitstream() {
	pkt_decode();
	inform_sender();
}

int write_payload() {

}
