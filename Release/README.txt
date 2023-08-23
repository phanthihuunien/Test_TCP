Demo chương trình: https://www.youtube.com/watch?v=ELDs7d4ZjXQ

Hướng dẫn chạy chương trình

Bước 1: Mở Command Promt. Chuyển đến thư mục bạn lưu file ReceiveData.exe, SendData.exe 
Bước 2: Ở chương trình ReceiveData gõ lệnh: ReceiveData --out <location_store_file>
	+ Với <location_store_file> là đường dẫn của thư mục sẽ lưu trữ tập tin nhận từ máy gửi
Bước 3: Ở chương trình SendData gõ lệnh: SendData <destination_address>
	+ Với  <destination_address> là địa chỉ của máy nhận. Chạy 2 chương trình trên cùng một máy thì <destination_address> là 127.0.0.1
Bước 4: Vẫn ở chương trình SendData:
	+ Để gửi đoạn văn bản, gõ lệnh: SendText <text>
		- Với <text> là đoạn văn bản cần gửi
	+ Để gửi tập tin, gõ lệnh: SendFile <path> <buffer-size> 
		- Với <path> là đường dẫn của file cần gửi trên máy gửi
		- Với <buffer-size> là kích thước của buffer gửi. Nếu bỏ trống thì sẽ tự động gửi với buffer bằng 1024.
	+ Gõ lệnh Exit để thoát chương trình.