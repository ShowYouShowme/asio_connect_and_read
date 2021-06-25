#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <functional>
#include <array>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <string>
#include <iostream>
#include <boost/format.hpp>
#include <chrono>
#include <stdint.h>
#include <system_error>
#include <tuple>
#include <vector>
#include <set>
using boost::asio::ip::tcp;


static std::string getDate()
{
	auto time = boost::posix_time::second_clock::local_time();
	std::string strPosixTime = boost::posix_time::to_iso_extended_string(time);
	return strPosixTime;
}

// 使用boost.asio 实现connect 的超时设置
// TODO 使用boost.asio 实现read 超时设置
//bool Connected = false;
//
//bool Received = false;
//
//// cb 注册了一次只能触发一次
//void connect_timeout(const boost::system::error_code & e, boost::asio::io_context* ptr)
//{
//	if (!Connected) {
//		std::cout << "connect timeout at : " << getDate() << std::endl;
//		ptr->stop();
//	}
//}
//
//void read_timeout(const boost::system::error_code & e, boost::asio::io_context* ptr)
//{
//	if (!Received) {
//		std::cout << "read timeout at : " << getDate() << std::endl;
//		ptr->stop();
//	}
//}
//// read_handler 注册了只能触发一次,函数执行后要再次注册
//void read_handler(const boost::system::error_code& error, std::size_t bytes_transferred, 
//	std::array<unsigned char, 2048>* pBuffer,
//	tcp::socket* pSocket)
//{
//	std::cout << "receive data at : " << getDate() << std::endl;
//	if (error) {
//		std::cout << error.message() << std::endl;
//		return;
//	}
//	Received = true;
//		
//	std::string output(pBuffer->begin(), pBuffer->begin() + bytes_transferred);
//	std::cout << "recv : " << output << std::endl;
//	
//	std::array<unsigned char, 2048>& buf = *pBuffer;
//	pSocket->async_read_some(boost::asio::buffer(*pBuffer, pBuffer->size()), std::bind(&read_handler, std::placeholders::_1, std::placeholders::_2, pBuffer, pSocket));
//}
//
//// connect time_out 和 read_timeout 设置的demo
//int main(int argc, char* argv[])
//{
//	const char* host = "172.18.80.183";
//	unsigned short port = 10006;
//	
//	boost::asio::io_context io_context;
//	boost::asio::ip::tcp::endpoint endpoint(
//	    boost::asio::ip::address::from_string(host),
//		port);
//
//	tcp::socket socket(io_context);
//	
//	const int READ_TIMEOUT		= 20;
//	const int CONNECT_TIMEOUT	= 15;
//	boost::asio::steady_timer t(io_context); // 不要放到connect 的回调里面,超出作用域后释放了
//	socket.async_connect(endpoint,
//		[&](const boost::system::error_code& error) {
//			Connected = true;
//			if (error) {
//				std::cout << error.message() << std::endl;
//			}
//			else {
//				std::cout << "connect success!" << std::endl;
//				
//				// 写入数据,然后异步读取相应,此处开始设置读取超时
//				char data[1024] = { 'j', 'u', 's', 't', 't', '\0' };
//				socket.write_some(boost::asio::buffer(data, 5));
//				
//				t.expires_after(boost::asio::chrono::seconds(READ_TIMEOUT));  // 设置10秒后超时
//				t.async_wait(std::bind(&read_timeout, std::placeholders::_1, &io_context));
//				std::array<unsigned char, 2048> buffer;
//				socket.async_read_some(boost::asio::buffer(buffer, buffer.size()), std::bind(&read_handler, std::placeholders::_1, std::placeholders::_2, &buffer, &socket));
//				std::cout << "begin at : " << getDate() << std::endl;
//			}
//		});
//
//	std::cout << "connect begin at : " << getDate() << std::endl;
//	// 设置连接超时
//	t.expires_after(boost::asio::chrono::seconds(CONNECT_TIMEOUT));   // 设置10秒后超时
//	t.async_wait(std::bind(&connect_timeout, std::placeholders::_1, &io_context));
//	io_context.run();
//	return 0;
//}

class Socket {
private:
	bool Connected = false;
	long long seqNum = 1; // 为每一次Connect 或者 Read 分配一个序号
	std::set<long long> container;
public:
	// 可以设定超时的连接
	std::error_code Connect(std::string& host, unsigned short port ,boost::asio::io_context& io_context, tcp::socket& socket, int time_out) {
		if (this->Connected) {
			return std::error_code();
		}
		
		long long runtimeId = this->seqNum;
		this->container.insert(this->seqNum);
		this->seqNum += 1;
		boost::asio::ip::tcp::endpoint endpoint(
		    boost::asio::ip::address::from_string(host),
			port);
		
		// 使用 runtimeId 限制  connect cb 和 connect_timeout 只有一个能被调用
		socket.async_connect(endpoint,
			[&, runtimeId](const boost::system::error_code& error) {
				if (this->container.count(runtimeId)) {
					this->container.erase(runtimeId);
					io_context.stop();
				
					if (error) {
						std::cout << error.message() << std::endl;
					}
					else {
						this->Connected = true;
						std::cout << "connect success!" << std::endl;
					}
				 }
			});
		
		// 这个函数一会儿还是会被调用,因此要根据runtimeId 过滤失效的操作
		auto connect_timeout = [&, runtimeId](const boost::system::error_code & e) {
			if (this->container.count(runtimeId)) {
				// 超时
				this->container.erase(runtimeId);
				if (this->Connected == false) {
					std::cout << "connect timeout at : " << getDate() << std::endl;
					io_context.stop();
				}
			 }
		};
		std::cout << "connect begin at : " << getDate() << std::endl;
		boost::asio::steady_timer t(io_context);
		t.expires_after(boost::asio::chrono::seconds(time_out));   // 设置连接超时
		t.async_wait(connect_timeout);
		io_context.run();
		std::cout << "connect begin at : " << getDate() << std::endl;
		if (!Connected) {
			return std::make_error_code(std::io_errc::stream);
		}
		else {
			return std::error_code(); // 成功
		}
			
	}
	
	std::tuple<std::string, std::error_code> Read(boost::asio::io_context& io_context, tcp::socket& socket, int time_out) {
		long long runtimeId = this->seqNum;
		this->container.insert(this->seqNum);
		this->seqNum += 1;
		
		bool stoped = io_context.stopped();
		bool occurError = false;
		std::string buffer(2048, '\0');
		
		// 使用 runtimeId 限制  read_timeout 和 read_handler 只有一个能被调用
		auto read_timeout = [&, runtimeId](const boost::system::error_code & e) {
			if (this->container.count(runtimeId)) {
				// 超时
				this->container.erase(runtimeId);
				std::cout << "read timeout at : " << getDate() << std::endl;
				io_context.stop();
				occurError = true;
			}
		};
		
		std::function<void(const boost::system::error_code&, std::size_t)> read_handler = [&, runtimeId](const boost::system::error_code& error, std::size_t bytes_transferred) {
			if (this->container.count(runtimeId)) {
				this->container.erase(runtimeId); // erase 后超时不能调用了
				std::cout << "receive data at : " << getDate() << std::endl;
				if (error) {
					std::cout << error.message() << std::endl;
					occurError = true;
					return;
				}
				buffer.resize(bytes_transferred);
				std::cout << "recv : " << buffer << std::endl;
				io_context.stop(); // 让run() 直接返回
				// 注释去掉后可以多次读取
				// socket.async_read_some(boost::asio::buffer(buffer, buffer.size()), read_handler);
			}
		};
		
		boost::asio::steady_timer t(io_context); 
		const int READ_TIMEOUT		= time_out;
		t.expires_after(boost::asio::chrono::seconds(READ_TIMEOUT));   // 设置超时
		t.async_wait(read_timeout);
		socket.async_read_some(boost::asio::buffer(buffer, buffer.size()), read_handler);
		std::cout << "begin at : " << getDate() << std::endl;
		io_context.run();
		if (occurError) {
			return std::make_tuple(std::string(), std::make_error_code(std::io_errc::stream));
		}
		else {
			return std::make_tuple(buffer, std::error_code());
		}
	}
public:
	Socket() {}
	~Socket() {}
};

int main()
{
	std::string host = "172.18.80.183";
	unsigned short port = 10006;
	boost::asio::io_context io_context;
	tcp::socket socket(io_context);
	
	auto so = Socket();
	auto ec = so.Connect(host, port, io_context, socket, 20);
	if (ec.value()) {
		std::cout << "connect timeout..." << std::endl;
	}
	else {
		std::cout << "connect sucess..." << std::endl;
	}
	
	char data[1024] = { 'j', 'u', 's', 't', 't', '\0' };
	socket.write_some(boost::asio::buffer(data, 5));
	
	io_context.restart();
	auto[message, ec2] = so.Read(io_context, socket, 30);
	if (ec2.value()) {
		std::cout << "read error..." << std::endl;
	}
	else {
		std::cout << "recv : " << message << std::endl;
	}
		
	return 0;
}
	

