/* Chapter 11 - Client/Server system. CLIENT VERSION.
	ClientNP - Connection-oriented client. Named Pipe version */
	/* Execute a command line (on the server) and display the response. */

	/*  The client creates a long-lived
		connection with the server (consuming a pipe instance) and prompts
		the user for the command to execute. */

		/* This program illustrates:
			1. Named pipes from the client side.
			2. Long-lived connections with a single request but multiple responses.
			3. Reading responses in server messages until the end of response is received.
		*/

		/* Special commands recognized by the server are:
			1. $Statistics: Give performance statistics.
			2. $ShutDownThread: Terminate a server thread.
			3. $Quit. Exit from the client. */

#include "Everything.h"
#include "ClientServer.h" /* Defines the resquest and response records */
# include <iostream>
using namespace std;

int main(int argc, char* argv[])
{
	
	HANDLE hNamedPipe = INVALID_HANDLE_VALUE; // �����ʿ��� ���� �ڵ� hNamedPipe ��  INVALID_HANDLE_VALUE�� �ʱ�ȭ�Ѵ�. 
	const char quitMsg[] = "$Quit"; // command line���� �Է¹����� Quit���� �ƴ��� �Ǵ��ϱ� ���� const ����
	char serverPipeName[MAX_PATH + 1]; // �����κ��� ������ �̸��� �ޱ� ���� ����
	REQUEST request; // Ŭ���̾�Ʈ�� ������ ��û�� request ����ü
	RESPONSE response;	// Ŭ���̾�Ʈ�� �������� ���� response ����ü
	DWORD nRead, nWrite, npMode = PIPE_READMODE_MESSAGE | PIPE_WAIT; // nRead, nWrite, npMode �ʱ�ȭ

	std::cout << "Ŭ���̾�Ʈ�� LocateServer �Լ��� ���� ������ ���� �������̸��� �޾ƿɴϴ�. " << endl;
	LocateServer(serverPipeName, MAX_PATH); // �Ķ���ͷ� serverPipeName�� MAX_PATH�� �Ѱ��ش�. 

	// hNamedPipe�� INVALID_HANDLE_VALUE�̸� ����ؼ� while���� �ݺ� (hNamedPipe ����������) 
	std::cout << "hNamedPipe�� ���������� ������ �ݺ��մϴ�." << endl;
	while (INVALID_HANDLE_VALUE == hNamedPipe) { /* Obtain a handle to a NP instance */
		
		/*
		* WaitNamedPipeA �Լ�: ������ �ð����� �������� ������ ��ٸ���. ������ �ð����� ������ ������ ������ �Բ� ��ȯ�Ѵ�. 
		* ConnectNamedPipe�� timeout �����̴�. ������ ���α׷����� ����Ѵ�.

		1. lpNamedPipeName: �������� �̸� (serverPipeName)
		2. nTimeOut: ��ٸ� �ð����� �и������� ������. (NMPWAIT_WAIT_FOREVER=������ ���� ������ ��� ��ٸ���)

		return value: ���� �ð����� ������ �����ϸ� 0���� ū ���� ��ȯ�Ѵ�. ���� ���� �ð����� ������ �̷������ �ʴ�ٸ� 0�� ��ȯ�Ѵ�. 
		*/
		std::cout << "�������� ������ ���������� ��� ��ٸ��ϴ�." << endl;
		if (!WaitNamedPipeA(serverPipeName, NMPWAIT_WAIT_FOREVER))
			ReportError("WaitNamedPipe error.", 2, TRUE);
		/*
		* CreateFileA �Լ�: �����̳� ����� ��ġ�� ����. ��ǥ���� ����� ��ġ�δ� ����, ���� ��Ʈ��, ���丮, �������� ��ũ, ����, �ܼ� ����, ���̺� ����̺�, ������ ���� �ִ�. 
		* �� �Լ��� �� ��ġ�� ������ �� �ִ� handle(�ڵ�)�� ��ȯ�Ѵ�.

		1. lpFileName: ������ �ϴ� ���� �̸� (serverPipeName)
		2. dwDesiredAccess: ���� ����� ����ϱ� ���ؼ� ����Ѵ�. �Ϲ������� GENERIC_READ, GENERIC_WRITE Ȥ�� GENERIC_READ|GENERIC_WRITE�� ����Ѵ�. (GENERIC_READ | GENERIC_WRITE)
		3. dwShareMode: ��ü�� ���� ����� �����Ѵ�. 0�� �����ϸ� ������ �� ���� ���°� �ǰ�, �ڵ��� ������ ������ �ٸ� ����� �����ϰ� �ȴ�. (0)
		4. lpSecurityAttributes: NULL ������ ������� ����
		5. dwCreationDisposition: ������ ��������� ����Ѵ�. (OPEN_EXISTING: ������ �����ϸ� ����. ���� �����̳� ��ġ�� �������� ������, ���� �ڵ�� ERROR_FILE_NOT_FOUND (2)�� �����Ѵ�.)
		6. dwFlagsAndAttributes: ������ ��Ÿ �Ӽ��� �����Ѵ�. (FILE_ATTRIBUTE_NORMAL: �ٸ� �Ӽ��� ������ �ʴ´�.)
		7. hTemplateFile: ������ ���Ͽ� ���� �Ӽ��� �����ϴ� ���ø� (NULL)

		return value : �����ϸ� ���Ͽ� ���� �ڵ��� ��ȯ�Ѵ�. �����ϸ� INVALID_HANDLE_VALUE�� ��ȯ�Ѵ�. 
		*/

		std::cout << "serverPipeName�� �����ϰ�, hNamedPipe�� ��ȯ ���� �����մϴ�." << endl;
		hNamedPipe = CreateFileA(serverPipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		std::cout << "hNamedPipe�� �� �Ѿ�Դ��� Ȯ���մϴ�. hNamedPipe ��⿡ ���������� �ٽ� ������ �ݺ��մϴ�." << endl;
	}

	/*  Read the NP handle in waiting, message mode. Note that the 2nd argument
	 *  is an address, not a value. Client and server may be on the same system, so
	 *  it is not appropriate to set the collection mode and timeout (last 2 args)
	 */
	 /* SetNamedPipeHandleState �Լ� : �������� �Ӽ��� �����Ű�� �Լ� 
	 * ������ �Ȱ��� ������ �����ϱ� ���ؼ� ���
	  1. hNamedPipe : ���������� ���� �Ӽ��� �����Ű�� ���� �ڵ� (hNamedPipe)
	  2. lpMode : ������ �ۡ����� ���� �Լ��� ���ϸ�忡 ���� �� ���� (&npMode)
	  3. lpMaxCollectionCount : ���۸��� �� �ִ� �ִ� ����Ʈ ũ��, Ŭ���̾�Ʈ�� ������ ���� PC�� ��� NULL�� ���� (NULL)
	  4. lpCollectDataTimeOut : ���۸��� ����ϴ� �ִ� �ð�(�и�������), Ŭ���̾�Ʈ�� ������ ���� PC�� ��� NULL�� ���� (NULL)
	  */
	if (!SetNamedPipeHandleState(hNamedPipe, &npMode, NULL, NULL))
		ReportError("SetNamedPipeHandleState error.", 2, TRUE);

	/* Prompt the user for commands. Terminate on "$quit". */
	request.rqLen = RQ_SIZE; // request �޼��� ������

	std::cout << "ConsolePrompt�� ���� command�� �Է¹޽��ϴ�. ���� �޼����� �Է¹��������� ������ �ݺ��մϴ�." << endl;
	// �Է¹��� command�� request.record�� ��. ���̴� MAX_RQRS_LEN - 1
	// �Է¹��� request.record�� quitMsg�� �ƴҰ�� while���� �ݺ��մϴ�.
	while (ConsolePrompt("\nEnter Command: ", (char*)request.record, MAX_RQRS_LEN - 1, TRUE)
		&& (strcmp((const char*)request.record, quitMsg) != 0)) {

		/* WriteFile �Լ�
		* - ���� �����͸� ���� �Լ��Դϴ�.
		 1. hFile: �����̳� I/O ��ġ�� �ڵ�.  ( hNamedPipe)
		 2. lpBuffer: �����͸� �����ϱ� ���� ���۸� ����Ű�� ������ ( &response)
		 3. nNumberOfBytesToRead: ����� ����Ʈ ũ�� (RQ_SIZE)
		 4. lpNumberOfByteRead: ����� ������ ����Ʈ�� ���� ���Ϲ޴� �μ� (&nWrite)
		 5. lpOverlapped: ���� ������̸� NULL�� ���
		 return value : �����ϸ� 0�� �ƴ� ���� �����Ѵ�. �Լ��� �����Ҷ� Ȥ�� �񵿱� ������� �Ϸ����� ���� 0�� ��ȯ�Ѵ�.
		 */
		std::cout << "������ request�� ������, ���� write �մϴ�." << endl;
		if (!WriteFile(hNamedPipe, &request, RQ_SIZE, &nWrite, NULL))
			ReportError("Write NP failed", 0, TRUE);

		/* Read each response and send it to std out */
		/* ReadFile �Լ�
		* - Ư�� ���� Ȥ�� ����� ��ġ�� ���� �����͸� �д´�.
		 1. hFile: ����, ���� ��Ʈ��, ������ũ, ������ ����̺�, ���ϰ� ���� ��ġ�� �ڵ�.  ( hNamedPipe)
		 2. lpBuffer: �����͸� �����ϱ� ���� ���۸� ����Ű�� ������ ( &response)
		 3. nNumberOfBytesToRead: ���� �ִ� ����Ʈ ũ�� (RS_SIZE)
		 4. lpNumberOfByteRead: ���� ����� ��忡��, �о���� �������� ����Ʈ ���� �ѱ�� (&nRead)
		 5. lpOverlapped: ���� ������̸� NULL�� ���
		 return value : �����ϸ� 0�� �ƴ� ���� �����Ѵ�. �Լ��� �����Ҷ� Ȥ�� �񵿱� ������� �Ϸ����� ���� 0�� ��ȯ�Ѵ�. 
		 */
		std::cout << "�������� ���� response�� �а�, ���� �����͸� ����մϴ�." << endl;
		while (ReadFile(hNamedPipe, &response, RS_SIZE, &nRead, NULL))
		{
			// �������� ���� response�� 1 �Ǵ� 0�̸� break
			if (response.rsLen <= 1)	/* 0 length indicates response end */
				break;
			printf("%s", response.record);
		}
	}

	
	std::cout << "Quit command�� �޾ҽ��ϴ�. Disconnect ��ŵ�ϴ�. " << endl;

	std::cout << "�ڵ� hNamedPipe�� close�մϴ�. " << endl;
	CloseHandle(hNamedPipe);
	return 0;
}
