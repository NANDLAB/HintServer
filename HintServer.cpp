#include <iostream>
#include <memory>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <cstdlib>
#include <iomanip>
#include <vlcpp/vlc.hpp>
#include <Magick++.h>
#include "wtok.h"
#include "wlaup.h"
#include "wmain.hpp"

using namespace std;
using boost::asio::ip::udp;
using namespace Magick;

int tmp_index = 0;

VLC::Instance instance = VLC::Instance(0, nullptr);
VLC::MediaPlayer mp = VLC::MediaPlayer(instance);

int cmd_showhint(int argc, wchar_t *argv[]);
int cmd_showbg(int argc, wchar_t *argv[]);
int cmd_playmedia(int argc, wchar_t *argv[]);
int cmd_exit(int argc, wchar_t *argv[]);
int add_caption(const wchar_t *image_source, const wchar_t *image_destination, const wchar_t *text);

struct Command {
	const wchar_t * name;
	int (*functptr) (int argc, wchar_t *argv[]);
};

const Command cmds[] = {
	{L"showhint",	cmd_showhint},
	{L"showbg",		cmd_showbg},
	{L"playmedia",	cmd_playmedia},
	{L"exit",		cmd_exit}
};

const int NCMDS = NELEMS(cmds);

enum {
	NTOKENS	=	16,
	ERROR_NOT_FOUND			=	127,
};

wstring mediadir; // Defaults to ~/Media/
wstring background = L"bg.jpg";
wstring hint_background = L"hbg.jpg";
unsigned port = LAUP_PORT;

int errorCode = ERROR_NONE;

int interpret(wchar_t * line) {
	size_t tokenc;
	bool found = false;
	wchar_t *tokenv[NTOKENS];
	int tokResult = w_tokenize(line, &tokenc, tokenv, NTOKENS);
	if (tokResult) {
		if (tokResult == ERROR_TOO_MANY_TOKENS) {
			wprintf(L"Shell: Max number of %d tokens exceeded!\n", NTOKENS);
		}
		if (tokResult == ERROR_INVALID_QUOTE) {
			wprintf(L"Shell: Unclosed quotation mark!\n");
		}
		if (tokResult == ERROR_INVALID_ESCAPE) {
			wprintf(L"Shell: Invalid escape sequence!\n");
		}
		return errorCode = tokResult;
	}
	if (!tokenc) {
		return errorCode = ERROR_NONE;
	}
	for (int i = 0; i < NCMDS; i++) {
		if (!wcscmp(tokenv[0], cmds[i].name)) {
			found = true;
			errorCode = cmds[i].functptr(tokenc, tokenv);
			break;
		}
	}
	if (!found) {
		wprintf(L"Shell: Command '%ls' not found!\n", tokenv[0]);
		errorCode = ERROR_NOT_FOUND;
	}
	return errorCode;
}

class udp_server {
public:
	static const int server_port = 40000;
	static const int buffer_size = 1024;

private:
	udp::socket socket_;
	udp::endpoint remote_endpoint_;
	char recv_buffer_[buffer_size];

	void start_receive() {
		socket_.async_receive_from(boost::asio::buffer(recv_buffer_, sizeof(recv_buffer_)), remote_endpoint_,
				boost::bind(&udp_server::handle_receive, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
		);
	}
	void handle_receive(const boost::system::error_code &error, size_t bytes_transferred) {
		start_receive();
		wcout << L"Listening for further commands..." << endl;
		wcout << L"Received size: " << bytes_transferred << endl;
		wcout << L"Received error code: " << error << endl;
		if (bytes_transferred == buffer_size) {
			wcout << L"The command is larger than " << buffer_size-1 << L" characters and can not be interpreted." << endl;
		}
		else {
			wchar_t w_recv_buffer_[bytes_transferred+1];
			recv_buffer_[bytes_transferred] = '\0';
			size_t conversion_result = mbstowcs(w_recv_buffer_, recv_buffer_, bytes_transferred+1);
			if (conversion_result == SIZE_MAX) {
				wcout << L"Invalid multibyte character sequence in command. It cannot be interpreted." << endl;
			}
			else {
				wcout << L"Received command: " << w_recv_buffer_ << endl;
				wcout << L"Interpreting received command." << endl;
				interpret(w_recv_buffer_);
			}
		}
	}
public:
	udp_server(boost::asio::io_context &io)
		:	socket_(io, udp::endpoint(udp::v4(), server_port))
	{
		start_receive();
		wcout << L"Listening for commands..." << endl;
	}
};

void vlcErrorHandler() {
	cout << "VLC encountered an error, terminating!" << endl;
	exit(EXIT_FAILURE);
}

int wmain(int argc, wchar_t *argv[]) {
	try {
		mp.setFullscreen(true);
		mp.eventManager().onEncounteredError(vlcErrorHandler);
		InitializeMagick(nullptr);
		char *home = getenv("HOME");
		if (!home) {
			wcout << L"Could not determine home directory!" << endl;
			exit(EXIT_FAILURE);
		}
		size_t homelen = strlen(home);
		wchar_t *whome = new wchar_t[homelen+1];
		mbstowcs(whome, home, homelen+1);
		mediadir = whome;
		delete [] whome;
		size_t whomelen = mediadir.size();
		if ((whomelen > 0) && (mediadir[whomelen-1] != L'/')) {
			mediadir += L'/';
		}
		mediadir += L"Media/";
		for (wchar_t **arg = argv+1; *arg != nullptr; arg++) {
			if (!(wcscmp(*arg, L"--help") && wcscmp(*arg, L"-?"))) {
				wcout << L"Help output." << endl;
				exit(EXIT_SUCCESS);
			}
			else if (!(wcscmp(*arg, L"--port") && wcscmp(*arg, L"-p"))) {
				if (++arg == nullptr) {
					wcout << L"Port value expected!" << endl;
					exit(EXIT_FAILURE);
				}
				int s_port;
				int n = swscanf(*arg, L"%i", &s_port);
				if (n < 1 || s_port < 0) {
					wcout << L"Specified port is not a valid unsigned integer!" << endl;
					exit(EXIT_FAILURE);
				}
				port = s_port;
			}
			else if (!(wcscmp(*arg, L"--mediadir") && wcscmp(*arg, L"-m"))) {
				if (++arg == nullptr) {
					wcout << L"Media directory expected!" << endl;
					exit(EXIT_FAILURE);
				}
				mediadir = *arg;
				size_t s = mediadir.size();
				if (s > 0 && mediadir[s-1] != L'/') {
					mediadir += L'/';
				}
			}
			else if (!(wcscmp(*arg, L"--background") && wcscmp(*arg, L"-b"))) {
				if (++arg == nullptr) {
					wcout << L"Background image path expected!" << endl;
					exit(EXIT_FAILURE);
				}
				background = *arg;
			}
			else if (!(wcscmp(*arg, L"--hint-background") && wcscmp(*arg, L"-h"))) {
				if (++arg == nullptr) {
					wcout << L"Hint background image path expected!" << endl;
					exit(EXIT_FAILURE);
				}
				hint_background = *arg;
			}
			else {
				wcout << L"Unrecognized option " << quoted(*arg) << L'!' << endl;
				exit(EXIT_FAILURE);
			}
		}
		/*
		wcout << port << endl;
		wcout << mediadir << endl;
		wcout << background << endl;
		wcout << hint_background << endl;
		*/

		wchar_t arg0[] = L"showbg";
		wchar_t *argv[] = {arg0, nullptr};
		cmd_showbg(1, argv);

		boost::asio::io_context io;
		udp_server server(io);
		io.run();
	}
	catch (exception &e) {
		wcerr << L"Terminating due to exception: " << e.what() << endl;
		exit(EXIT_FAILURE);
	}
	catch (...) {
		wcerr << L"Terminating due to exception of undetermined type." << endl;
		exit(EXIT_FAILURE);
	}
	return EXIT_SUCCESS;
}

int cmd_showhint(int argc, wchar_t *argv[]) {
	if (argc < 2) {
		wcout << L"Usage: showhint HINT!" << endl;
		return 1;
	}
	try {
		size_t hlen = wcslen(argv[1]);
		char *c_hint = new char[hlen+1];
		wcstombs(c_hint, argv[1], hlen+1);
		string hint = c_hint;
		delete [] c_hint;

		wstring w_hbg = mediadir + hint_background;
		size_t hbg_len = wcslen(w_hbg.data());
		char *c_hbg = new char[hbg_len+1];
		wcstombs(c_hbg, w_hbg.data(), hbg_len+1);
		string hbg = c_hbg;
		delete [] c_hbg;

		wostringstream w_out;
		w_out << mediadir << L"tmp" << tmp_index << L".png";
		size_t outlen = w_out.str().size();
		char *c_out = new char[outlen+1];
		wcstombs(c_out, w_out.str().data(), outlen+1);
		string out = c_out;
		delete [] c_out;

		tmp_index = !tmp_index;

		const Color black = Color(0, 0, 0, OpaqueOpacity);
		const Color white = Color(QuantumRange, QuantumRange, QuantumRange, OpaqueOpacity);
		const Color red = Color(MaxRGB, 0, 0, OpaqueOpacity);
		const Color transparent = Color(MaxRGB, MaxRGB, MaxRGB, TransparentOpacity);

		Image image;
		image.read(hbg);

		Geometry geo = image.size();

		const double interline_space = 60;
		const double font_size = 80;

		Image caption_outline(geo, transparent);
		caption_outline.backgroundColor(transparent);
		caption_outline.textGravity(CenterGravity);
		caption_outline.textInterlineSpacing(interline_space);
		caption_outline.fontPointsize( font_size );
		caption_outline.fillColor( black );
		caption_outline.strokeWidth( 1000.0 );
		caption_outline.strokeColor( black );
		caption_outline.read("CAPTION:" + hint);

		Image caption_fill(geo, transparent);
		caption_fill.textInterlineSpacing(interline_space);
		caption_fill.backgroundColor(transparent);
		caption_fill.textGravity(CenterGravity);
		caption_fill.fontPointsize( font_size );
		caption_fill.fillColor( white );
		caption_fill.strokeColor( white );
		caption_fill.strokeWidth( 0 );
		caption_fill.read("CAPTION:" + hint);

		image.composite(caption_outline, 0, 0, OverCompositeOp);
		image.composite(caption_fill, 0, 0, OverCompositeOp);

		image.write(out);

		VLC::Media media = VLC::Media(instance, out.data(), VLC::Media::FromPath);
	    mp.pause();
	    mp.setMedia(media);
	    mp.play();
	}
	catch( Exception &error_ )
	{
	  cout << "Caught ImageMagick exception: " << error_.what() << endl;
	  return 1;
	}
	return 0;
}
int cmd_showbg(int argc, wchar_t *argv[]) {
	wstring bg = mediadir + background;
    size_t bglen = bg.size();
	char *mbs = new char [bglen+1];
	wcstombs(mbs, bg.data(), bglen+1);
	VLC::Media media = VLC::Media(instance, mbs, VLC::Media::FromPath);
	mp.pause();
	mp.setMedia(media);
    mp.play();
	delete [] mbs;
	return 0;
}
int cmd_playmedia(int argc, wchar_t *argv[]) {
	if (argc < 2) {
		wcout << L"Usage: playmedia MEDIA!" << endl;
		return 1;
	}
	wstring m = mediadir + argv[1];
    size_t mlen = m.size();
	char *mbs = new char [mlen+1];
	wcstombs(mbs, m.data(), mlen+1);
	VLC::Media media = VLC::Media(instance, mbs, VLC::Media::FromPath);
	mp.pause();
	mp.setMedia(media);
    mp.play();
	delete [] mbs;
	return 0;
}
int cmd_exit(int argc, wchar_t *argv[]) {
	wcout << "Bye!" << endl;
	exit(EXIT_SUCCESS);
	return 0;
}
int add_caption(const wchar_t *image_source, const wchar_t *image_destination, const wchar_t *text) {
	return 0;
}
