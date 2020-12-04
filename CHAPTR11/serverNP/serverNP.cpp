/*  Chapter 11. ServerNP.
 *	Multi-threaded command line server. Named pipe version
 *	Usage:	Server [UserName GroupName]
 *	ONE THREAD AND PIPE INSTANCE FOR EVERY CLIENT. */

#include "Everything.h"
#include "ClientServer.h" /* Request and Response messages defined here */
#include <VersionHelpers.h>
# include <iostream>
using namespace std;

// Ŭ���̾�Ʈ �ϳ��� ������ 1��. ��, ���� ������ ����ü 1��
typedef struct {				/* Argument to a server thread. */
	HANDLE hNamedPipe;	 // �����ʿ��� �޾ƾ��ϴ� named pipe �ڵ�
	DWORD threadNumber; // ������ ����
	char tempFileName[MAX_PATH]; // ���� �������� �ӽ� ���� �̸�
} THREAD_ARG;

typedef THREAD_ARG * PTHREAD_ARG; // THREAD_ARG *�� PTHREAD_ARG�� typedef ������


volatile static LONG shutDown = 0; //���� �Ǿ�� �Ҷ� 1�� �ٲ�� ���ؼ� shutDown�� 0���� ����
static DWORD WINAPI Server(PTHREAD_ARG); // Server �Լ� ����
static DWORD WINAPI Connect(PTHREAD_ARG); // Connect �Լ� ����
static DWORD WINAPI ServerBroadcast(LPLONG); // ServerBroadcast �Լ� ����
static BOOL  WINAPI Handler(DWORD); // Handler �Լ� ���� (ctrl+c�� ������ ���� �Ǵ°� ���� ����)
static CHAR shutRequest[] = "$ShutDownServer"; // shutRequest[] �ʱ�ȭ 
static THREAD_ARG threadArgs[MAX_CLIENTS]; // threadArgs ���� (MAX_CLIENTS�� 4)

int main(int argc, char *argv[])
{
	HANDLE hNp, hMonitor, hSrvrThread[MAX_CLIENTS]; // �� hNP, hMonitor, hSrvrThread �ڵ� ����
	DWORD iNp; // Named pipe index iNp ����
	UINT monitorId, threadId;  // monitorId, threadId ����
	DWORD AceMasks[] =	/* Named pipe access rights - described in Chapter 15 */
	{ STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0X1FF, 0, 0 };
	LPSECURITY_ATTRIBUTES pNPSA = NULL;  // pNPSA�� NULL�� �ʱ�ȭ (����� NULL�μ���, ��, ������ �������� ��� ���� ���� �ǹ�)

	//  ������ 8���� �Ʒ��� ���� ���
	if (!IsWindows8OrGreater())
		ReportError("You need at least Windows 8. Your Version is Not Supported", 1, FALSE);

	/* Console control handler to permit server shut down by Ctrl-C or Ctrl-Break */
	/*
	 SetConsoleCtrlHandler �Լ�
	 application-defined HandlerRoutine function�� �߰��Ҷ� ���(��Ʈ�� �ڵ鷯 ���)
	 ó���� �Լ��� ���� ���� ������ �Լ��� ȣ�� ���μ������� CTRL + C ��ȣ�� ���� �ϴ��� ���θ� ���� �ϴ� ��� ������ Ư���� ���� �մϴ�.

	 1. HandlerRoutine: �߰� �ϰų� ������ ���� ���α׷� ���� HandlerRoutine �Լ��� �� �� ������
	 2. Add: �� �Ű� ������ TRUE �̸� ó���Ⱑ �߰� �˴ϴ�. FALSE �̸� ó���Ⱑ ���� �˴ϴ�. (TRUE: �߰�)

	 return value: �Լ��� ���� �ϸ� 0�� �ƴ� ���� ��ȯ �˴ϴ�.

	 */
	if (!SetConsoleCtrlHandler(Handler, TRUE))
		ReportError("Cannot create Ctrl handler", 1, TRUE);
	cout << "Ctrl handler ������ �����߽��ϴ�." << endl;


	cout << "thread broadcast pipe name�� �����մϴ�. " << endl;
	// _beginthreadex: ������ ����
		//  -> C / C++ Runtime-Library ���� ����
		//  -> ���������� ���� ������ �������� �ڵ��� ���� �ʱ� ������ ��������� ::CloseHandle( ) �Լ��� ȣ���Ͽ� �������� �ڵ��� �������� �ݾ� �־�� �Ѵ�.
		//  -> �����带 ������ �� ȣ���ϴ� �Լ��̴�.
		// <�Ķ����>
		//  1. void *security: �����Ϸ��� �������� ���ȿ� ���õ� ������ ���� �ʿ��� �ɼ� (NULL�� ����)
		//  2. unsigned stack_size: �����带 �����ϴ� ���, ��� �޸� ������ ���� ������ ���������� �����ȴ�. ������, ���� �� �䱸�Ǵ� ������ ũ�⸦ ���ڷ� �����Ѵ�. (0: ����Ʈ�� �����Ǿ� �ִ� ������ ũ��)
		//	3. unsigned (*start_address)(void*): �����忡 ���� ȣ��Ǵ� �Լ��� �����͸� ���ڷ� ����(ServerBroadcast)
		//	4. void* arglist: lpStartAddress �� ����Ű�� �Լ� ȣ�� ��, ������ ���ڸ� ���� (NULL)
		//	5. unsigned initflag:  ���ο� ������ ���� ���Ŀ� �ٷ� ���� ������ ���°� �Ǵ���, �ƴϸ� ��� ���·� �����ĸ� ���� (0: ��� ����)
		//	6. unsigned* thrdaddr: ������ ���� �� ������id�� ���ϵǴµ�, �̸� ���� (monitorId)
		// return: ������  ���� ���� �����忡 �ڵ��� ��ȯ, ���н� 0 ��ȯ
	hMonitor = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)ServerBroadcast, NULL, 0, &monitorId);
	


	/*	Create a pipe instance for every server thread.
	 *	Create a temp file name for each thread.
	 *	Create a thread to service that pipe. */

	cout << "��� ���� �����帶�� pipe instance, temp file name�� �����մϴ�." << endl;
	cout << "MAX_CLIENTS=4��ŭ �����带 �����մϴ�." << endl;
	for (iNp = 0; iNp < MAX_CLIENTS; iNp++) {
		/*
		CreateNamedPipeA: CreateNamedPipe�� ���� Named Pipe�� �ν��Ͻ��� �����ϰ� �������� �����ϱ� ���� �ڵ鷯�� ��ȯ�ϴ� �Լ�
		1. lpName: ������ �������� �̸����� ������ ������ ���� ("\\\\.\\PIPE\\SERVER")
		 2. dwOpenMode: �������� ���� ��İ� ���� ����.  PIPE_ACCESS_DUPLEX�� �б�&���� ���� ������ Ŭ���̾�Ʈ�� ��������� �а� �� �� �ִ�. ������ GENERIC_READ, GENERIC_WRITE ���� ���ٱ��Ѱ� ������ ȿ���� ������
		 3. dwPipeMode: �������� ������ ��� ����� ����. PIPE_READMODE_MESSAGE | PIPE_TYPE_MESSAGE | PIPE_WAIT
		 4. nMaxInstances: ������ �� �ִ� �ִ� ������ ���� -> MAX_CLIENTS = 4�� ����
		 5. nOutBufferSize: ��� ���� ������. 0�� default ���
		 6. nInBufferSize: �Է� ���� ������. 0�� default ���
		 7. nDefaultTimeout:  INFINITE�� ����
		 8. lpSecurityAttributes: ���ȼӼ� (pNPSA�� ����)
		 return value : ���н� INVALID_HANDLE_VALUE ����
		*/
		cout << "named pipe�� �����մϴ�." << endl;
		hNp = CreateNamedPipeA(SERVER_PIPE, PIPE_ACCESS_DUPLEX,
			PIPE_READMODE_MESSAGE | PIPE_TYPE_MESSAGE | PIPE_WAIT,
			MAX_CLIENTS, 0, 0, INFINITE, pNPSA);

		cout << "named pipe ������ ���������� ������ ����մϴ�." << endl;
		if (hNp == INVALID_HANDLE_VALUE)
			ReportError("Failure to open named pipe.", 1, TRUE);
		cout << "named pipe ������ �Ϸ��߽��ϴ�." << endl;

		// ���� ������� ���� ������ ����� �� �� �����Ƿ� �����忡�� Ÿ���ڵ��� �ѱ��.
		threadArgs[iNp].hNamedPipe = hNp;  // threadArgs[iNp].hNamedPipe�� hNp�� �Ҵ�
		threadArgs[iNp].threadNumber = iNp; // threadArgs[iNp].threadNumber�� iNP�� �Ҵ�

		/*
		* GetTempFileNameA: ��ũ�� ũ�Ⱑ 0����Ʈ�� ������ �̸��� �ӽ� ������ ����� �ش� ������ ��ü ��θ� ��ȯ�մϴ�.
		 1. lpPathName: ���� directory path�� ���� (".")
		 2. lpPrefixString: prefix string ����. "CLP"
		 3. uUnique: �ӽ� ������ ������ �� ���Ǵ� unsigned int(0���� �����ؼ� ���� �ý��� �ð��� �̿��ؼ� unique file name�� ����)
		 4. lpTempFileName: �ӽ� �����̸��� �޴� ������ ������ (threadArgs[iNp].tempFileName�� ����) 
		 */

		GetTempFileNameA(".", "CLP", 0, threadArgs[iNp].tempFileName);

		// _beginthreadex: ������ ����
		//  -> C / C++ Runtime-Library ���� ����
		//  -> ���������� ���� ������ �������� �ڵ��� ���� �ʱ� ������ ��������� ::CloseHandle( ) �Լ��� ȣ���Ͽ� �������� �ڵ��� �������� �ݾ� �־�� �Ѵ�.
		//  -> �����带 ������ �� ȣ���ϴ� �Լ��̴�.
		// <�Ķ����>
		//  1. void *security: �����Ϸ��� �������� ���ȿ� ���õ� ������ ���� �ʿ��� �ɼ� (NULL�� ����)
		//  2. unsigned stack_size: �����带 �����ϴ� ���, ��� �޸� ������ ���� ������ ���������� �����ȴ�. ������, ���� �� �䱸�Ǵ� ������ ũ�⸦ ���ڷ� �����Ѵ�. (0: ����Ʈ�� �����Ǿ� �ִ� ������ ũ��)
		//	3. unsigned (*start_address)(void*): �����忡 ���� ȣ��Ǵ� �Լ��� �����͸� ���ڷ� ����(Server)
		//	4. void* arglist: lpStartAddress �� ����Ű�� �Լ� ȣ�� ��, ������ ���ڸ� ���� (threadArgs[iNp])
		//	5. unsigned initflag:  ���ο� ������ ���� ���Ŀ� �ٷ� ���� ������ ���°� �Ǵ���, �ƴϸ� ��� ���·� �����ĸ� ���� (0: ��� ����)
		//	6. unsigned* thrdaddr: ������ ���� �� ������id�� ���ϵǴµ�, �̸� ���� (threadId)
		// return: ������  ���� ���� �����忡 �ڵ��� ��ȯ, ���н� 0 ��ȯ
		cout << "���� �����带 �����մϴ�. ���н� ������ ����մϴ�." << endl;
		hSrvrThread[iNp] = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)Server,
			&threadArgs[iNp], 0, &threadId);


		if (hSrvrThread[iNp] == NULL)
			ReportError("Failure to create server thread.", 2, TRUE);
		cout << "���� ������ ������ �����߽��ϴ�." << endl;
	}

	cout << "������ 4���� ��� �����߽��ϴ�." << endl;
	cout << "4���� �����尡 ��� �����⸦ ������ ��ٸ��ϴ�." << endl;
	WaitForMultipleObjects(MAX_CLIENTS, hSrvrThread, TRUE, INFINITE);
	printf("All Server worker threads have shut down.\n");
	cout << "��� ���� �����尡 ����Ǿ����ϴ�. " << endl;

	cout << "����� �����尡 ���������� ��ٸ��ϴ�." << endl;
	WaitForSingleObject(hMonitor, INFINITE);
	cout << "����� �����尡 ����Ǿ����ϴ�." << endl;
	printf("Monitor thread has shut down.\n");

	CloseHandle(hMonitor); // ����� �ڵ� close

	// ��� ������ �ڵ���� �ݰ� �ӽ� ���ϵ� ������
	for (iNp = 0; iNp < MAX_CLIENTS; iNp++) {
		/* Close pipe handles and delete temp files */
		/* Closing temp files is redundant, as the worker threads do it */
		CloseHandle(hSrvrThread[iNp]);
		DeleteFileA(threadArgs[iNp].tempFileName);
	}

	printf("Server process will exit.\n");
	return 0;
}


static DWORD WINAPI Server(PTHREAD_ARG pThArg)

/* Server thread function. There is a thread for every potential client. */
{
	/* Each thread keeps its own request, response,
		and bookkeeping data structures on the stack.
		Also, each thread creates an additional "connect thread"
		so that the main worker thread can test the shut down flag
		periodically while waiting for a client connection. */

	HANDLE hNamedPipe, hTmpFile = INVALID_HANDLE_VALUE, hConTh = NULL, hClient; // �� hNamedPipe, hTmpFile, hConTh, hClient �ڵ���� ���� �� �ʱ�ȭ
	DWORD nXfer, conThStatus, clientProcessId; // nXfer, conThStatus, clientProcessId ����
	STARTUPINFO startInfoCh; // STARTUPINFO ����ü startInfoCh����
	SECURITY_ATTRIBUTES tempSA = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE }; // ���� �Ӽ� ����
	PROCESS_INFORMATION procInfo; // ���μ��� info ����ü ����
	FILE *fp; // ���� ���½� ����ϴ� ����
	REQUEST request; // request ���� ����
	RESPONSE response; // response ���� ����
	char clientName[256]; //clientName ���� ����

	GetStartupInfo(&startInfoCh);  // startInfoCh�� ǥ�� ����� ��ġ�� ���� �ڵ��� ����
	hNamedPipe = pThArg->hNamedPipe; // hNamedPipe�� pThArg->hNamedPipe �Ҵ�

	cout << "startInfoCh�� ���� ����� ���μ����� ������ �����ɴϴ�." << endl;
	// shutDown�� 0�ε��� ������ �ݺ���
	while (!shutDown) { 	/* Connection loop */

		/* Create a connection thread, and wait for it to terminate */
		/* Use a timeout on the wait so that the shut down flag can be tested */
		// _beginthreadex: hConTh ������ ����
		//  -> C / C++ Runtime-Library ���� ����
		//  -> ���������� ���� ������ �������� �ڵ��� ���� �ʱ� ������ ��������� ::CloseHandle( ) �Լ��� ȣ���Ͽ� �������� �ڵ��� �������� �ݾ� �־�� �Ѵ�.
		//  -> �����带 ������ �� ȣ���ϴ� �Լ��̴�.
		// <�Ķ����>
		//  1. void *security: �����Ϸ��� �������� ���ȿ� ���õ� ������ ���� �ʿ��� �ɼ� (NULL�� ����)
		//  2. unsigned stack_size: �����带 �����ϴ� ���, ��� �޸� ������ ���� ������ ���������� �����ȴ�. ������, ���� �� �䱸�Ǵ� ������ ũ�⸦ ���ڷ� �����Ѵ�. (0: ����Ʈ�� �����Ǿ� �ִ� ������ ũ��)
		//	3. unsigned (*start_address)(void*): �����忡 ���� ȣ��Ǵ� �Լ��� �����͸� ���ڷ� ����(Connect)
		//	4. void* arglist: lpStartAddress �� ����Ű�� �Լ� ȣ�� ��, ������ ���ڸ� ���� (pThArg)
		//	5. unsigned initflag:  ���ο� ������ ���� ���Ŀ� �ٷ� ���� ������ ���°� �Ǵ���, �ƴϸ� ��� ���·� �����ĸ� ���� (0: ��� ����)
		//	6. unsigned* thrdaddr: ������ ���� �� ������id�� ���ϵǴµ�, �̸� ���� (NULL)
		// return: ������  ���� ���� �����忡 �ڵ��� ��ȯ, ���н� 0 ��ȯ
		hConTh = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)Connect, pThArg, 0, NULL);

		cout << "Connect �����带 �����մϴ�. ���н� ������ ����մϴ�." << endl;
		if (hConTh == NULL) {
			ReportError("Cannot create connect thread", 0, TRUE);
			_endthreadex(2);
		}
		cout << "Connect ������ ������ �����߽��ϴ�." << endl;

		/* Wait for a client connection. */
		// Connect ������ �ڵ��� 5�ʵ��� signaled state�ɶ����� ��ٸ�
		cout << "client connection�� ��ٸ��ϴ�." << endl;
		while (!shutDown && WaitForSingleObject(hConTh, CS_TIMEOUT) == WAIT_TIMEOUT)
		{ /* Empty loop body */
		}

		// shutDown�� 0�ΰ�� continue, ���° �����尡 sutdown�Ȱ��� �����
		if (shutDown) printf("Thread %d received shut down\n", pThArg->threadNumber);
		if (shutDown) continue;	/* Flag could also be set by a different thread */

		CloseHandle(hConTh); hConTh = NULL; // hConTh�ڵ��� ����
		/* A connection now exists */

		/* GetNamedPipeClientComputerNameA
		* connection�� �����ϸ�  GetNamedPipeClientComputerNameA�� ���� named pipe�� ���� client computer name�� ���´�.
		1. Pipe: named pipe pThArg -> hNamedPipe
		2. ClientComputerName: ��ǻ�� �̸� clientName
		3. ClientComputerNameLength: ClientComputerName ���� ������
		Ŭ���̾�Ʈ�� Ŭ���̾�Ʈ �̸��� ������
		*/
		cout << "client computer name�� ������, ���н� clientName�� localhost�� �Ҵ��մϴ�. " << endl;
		if (!GetNamedPipeClientComputerNameA(pThArg->hNamedPipe, clientName, sizeof(clientName))) {
			strcpy_s(clientName, sizeof(clientName) / sizeof(CHAR) - 1, "localhost"); // clientName�� localhost�� ä����
		}

		/* GetNamedPipeClientProcessId
		 * named pipe�� ���� client ���μ��� id�� ���´�.
		 1. Pipe: named pipe�� ���� �ڵ� PThArg->hNamedPipe
		 2. ClientProcessId: process identifier�� &clientProcessId
		 */
		GetNamedPipeClientProcessId(pThArg->hNamedPipe, &clientProcessId);
		printf("Connect to client process id: %d on computer: %s\n", clientProcessId, clientName);

		/* ReadFile �Լ�
		* - Ư�� ���� Ȥ�� ����� ��ġ�� ���� �����͸� �д´�.
		 1. hFile: ����, ���� ��Ʈ��, ������ũ, ������ ����̺�, ���ϰ� ���� ��ġ�� �ڵ�.  ( hNamedPipe)
		 2. lpBuffer: �����͸� �����ϱ� ���� ���۸� ����Ű�� ������ ( &request)
		 3. nNumberOfBytesToRead: ���� �ִ� ����Ʈ ũ�� (RQ_SIZE)
		 4. lpNumberOfByteRead: ���� ����� ��忡��, �о���� �������� ����Ʈ ���� �ѱ�� (&nXfer)
		 5. lpOverlapped: ���� ������̸� NULL�� ���
		 return value : �����ϸ� 0�� �ƴ� ���� �����Ѵ�. �Լ��� �����Ҷ� Ȥ�� �񵿱� ������� �Ϸ����� ���� 0�� ��ȯ�Ѵ�.
		 */
		// shutDown�� 0�� �ƴϰ� ReadFile�� ���� ������ �о����� ������ �ݺ��Ѵ�.
		while (!shutDown && ReadFile(hNamedPipe, &request, RQ_SIZE, &nXfer, NULL)) {
			// client�� ������ ���涧���� ���ο� command�� ����ؼ� �޴°�
			printf("Command from client thread: %d. %s\n", clientProcessId, request.record); 

			shutDown = shutDown || (strcmp((const char *)request.record, shutRequest) == 0); //record�ȿ� ���� ����� shutRequest���� ���Ѱſ� shutdown ���� �ϳ��� 1�̸� shutdown�� 1�� ��
			if (shutDown)  continue; // shutdown�� 1�̸� continue

			/* Open the temporary results file used by all connections to this instance. */
			/* 
			* CreateFileA �Լ�: �����̳� ����� ��ġ�� ����. ��ǥ���� ����� ��ġ�δ� ����, ���� ��Ʈ��, ���丮, �������� ��ũ, ����, �ܼ� ����, ���̺� ����̺�, ������ ���� �ִ�.
			* �� �Լ��� �� ��ġ�� ������ �� �ִ� handle(�ڵ�)�� ��ȯ�Ѵ�.
			* ThArg->tempFileName�� ���ؼ� �ӽ� ���� open
			1. lpFileName: ������ �ϴ� ���� �̸� (pThArg->tempFileName)
			2. dwDesiredAccess: ���� ����� ����ϱ� ���ؼ� ����Ѵ�. �Ϲ������� GENERIC_READ, GENERIC_WRITE Ȥ�� GENERIC_READ|GENERIC_WRITE�� ����Ѵ�. (GENERIC_READ | GENERIC_WRITE)
			3. dwShareMode: ��ü�� ���� ����� �����Ѵ�. 0�� �����ϸ� ������ �� ���� ���°� �ǰ�, �ڵ��� ������ ������ �ٸ� ����� �����ϰ� �ȴ�. (FILE_SHARE_READ | FILE_SHARE_WRITE)
			4. lpSecurityAttributes: &tempSA
			5. dwCreationDisposition: ������ ��������� ����Ѵ�. (CREATE_ALWAYS)
			6. dwFlagsAndAttributes: ������ ��Ÿ �Ӽ��� �����Ѵ�. (FILE_ATTRIBUTE_TEMPORARY)
			7. hTemplateFile: ������ ���Ͽ� ���� �Ӽ��� �����ϴ� ���ø� (NULL)

			return value : �����ϸ� ���Ͽ� ���� �ڵ��� ��ȯ�Ѵ�. �����ϸ� INVALID_HANDLE_VALUE�� ��ȯ�Ѵ�.
			*/
			hTmpFile = CreateFileA(pThArg->tempFileName, GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE, &tempSA,
				CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);

			// �ӽ����� open�� ���н� ���� �����
			if (hTmpFile == INVALID_HANDLE_VALUE) { /* About all we can do is stop the thread */
				ReportError("Cannot create temp file", 0, TRUE);
				_endthreadex(1); // ������ ����
			}

			/* Main command loop */
			/* Create a process to carry out the command. */
			startInfoCh.hStdOutput = hTmpFile ; // startInfoCh.hStdOutput�� hTmpFile �Ҵ�
			startInfoCh.hStdError = hTmpFile; // hTmpFile�� startInfoCh ����ü�� hStdError�� �Ҵ�
			startInfoCh.hStdInput = GetStdHandle(STD_INPUT_HANDLE);//ǥ�� �Է� ��ġ�� ���� �ڵ��� startInfoCh.hStdInput�� �Ҵ�
			startInfoCh.dwFlags = STARTF_USESTDHANDLES; // ���μ����� ���� ǥ�� ��� �ڵ��� �ǵ��� startInfoCh.dwFlags ����

			/*
			* CreateProcessA: ���μ��� ����
			*  ���μ��� ������ ������ ��� ���� ����ϰ�  procInfo.hProcess�� NULL�� ����
			1. lpApplicationName : ���μ����� ������ ���� ������ �̸� (NULL�� ����)
			2. lpCommandLine : ����� �μ�.  request.record �� ����
			3. pProcessAttributes : ���μ��� ���� �Ӽ� NULL�� ���� = �⺻ ���� ��ũ����
			4. lpThreadAttributes : ������ ���ȼӼ� NULL�� ���� = �⺻ ���� ��ũ���� ���
			5. bInheritHandles : �ڽ� ���μ����� ������ �� ���� ��ũ���� �������� ��ӽ��� �� ������ ���� (TRUE: ��Ӱ���)
			6. dwCreationFlags : ���μ����� �÷��׸� 0���� ����
			7. lpEnvironment : ������ ���μ����� ����� ȯ�� ������ �����͸� NULL�� ������ �θ� ���μ����� ȯ�� ����� ��ӹ���
			8. lpCurrentDirectory : &startInfoCh�� ����
			9. lpProcessInformation: &procInfo�� ����
			*/
			// ���μ��� ���� -> ���μ��� ������ ������ ��� ���� �޼��� ��� �� procInfo�� hProcess�� NULL�� ����
			if (!CreateProcessA(NULL, (LPSTR)request.record, NULL,
				NULL, TRUE, /* Inherit handles. */
				0, NULL, NULL, (LPSTARTUPINFOA)&startInfoCh, &procInfo)) {
				PrintMsg(hTmpFile, "ERR: Cannot create process.");
				procInfo.hProcess = NULL;
			}

			CloseHandle(hTmpFile); // hTmpFile �ڵ��� ����

			// ���� ���μ����� running ���̶�� (procInfo.hProcesss�� NULL�� �ƴ϶��) => �� ����� ����� �� �� !
			if (procInfo.hProcess != NULL) { /* Server process is running */
				CloseHandle(procInfo.hThread); // procInfo.hThread ������ �ڵ� ����
				WaitForSingleObject(procInfo.hProcess, INFINITE); // procInfo.hProcess�� signaled state���� ������ ���
				CloseHandle(procInfo.hProcess); //  signaled state �Ǹ� ���μ��� �ڵ� ����
			}

			/* Respond a line at a time. It is convenient to use
				C library line-oriented routines at this point. */
			// ���μ��� �������ϱ� ����� �ӽ� ���� �ȿ� �� ����. ���� �� �ӽ������� �о��� (Ŭ���̾�Ʈ�� �ѱ�� ����)
			if (fopen_s(&fp, pThArg->tempFileName, "r") != 0) {
				printf("Temp output file is: %s.\n", pThArg->tempFileName);
				perror("Failure to open command output file.");
				break;  /* Try the next command. */
			}

			/* Avoid an "information discovery" security exposure. */
			/* ZeroMemory(&response, sizeof(response));  */
			// ���ڿ��� �ִ����MAX_RQRS_LEN= 4096��ŭ �о response.record�� ä��
			while (fgets((char *)response.record, MAX_RQRS_LEN, fp) != NULL) {
				response.rsLen = (LONG32)(strlen((const char *)response.record) + 1); // +1 ���ִϱ� 1�̸� NULL�ΰ�
				WriteFile(hNamedPipe, &response, response.rsLen + sizeof(response.rsLen), &nXfer, NULL); // �ӽ� ����� ������� hNamedPipe���� ���� ������ ����
			}
			/* Write a terminating record. Messages use 8-bit characters, not UNICODE */
			response.record[0] = '\0'; // response.record[0]�� NULL�� �Ҵ�
			response.rsLen = 0; //response.rsLen�� 0���� �Ҵ�
			WriteFile(hNamedPipe, &response, sizeof(response.rsLen), &nXfer, NULL); // response �����Ͱ� ���۵Ǵ� ������ sizeof(response.rsLen)��ŭ hNamedPipe�� ä��

			FlushFileBuffers(hNamedPipe); // FlushFileBuffers�� ���� hNamedPipe ���
			fclose(fp); // fp ����

		}   /* End of main command loop. Get next command */

		/* Client has disconnected or there has been a shut down requrest */
		/* Terminate this client connection and then wait for another */
		FlushFileBuffers(hNamedPipe); // FlushFileBuffers�� ����  �ٽ� �ѹ� hNamedPipe ���
		DisconnectNamedPipe(hNamedPipe); // ����� ������ Ŭ���̾�Ʈ�� �ڵ��� ���� (������ �������� �����ϴ� �Լ�)
	}

	/*  Force the connection thread to shut down if it is still active */
	cout << "Connect �����尡 ���� ���� �ʾҴ��� Ȯ���մϴ�." << endl;
	if (hConTh != NULL) {
		GetExitCodeThread(hConTh, &conThStatus); // hConTh�� exit code�� ����
		if (conThStatus == STILL_ACTIVE) { 
			// ������ active�̸�
			// ���� Connect�� ConnectNamedPipe�� ������ �ȵȰ����� �Ǵ��ؼ� �����Ű�� ���ؼ� create file�� ��¥�� ������ 

			/*
			* CreateFileA �Լ�: SERVER_PIPE�� ���ؼ� open
			1. lpFileName: ������ �ϴ� ���� �̸� SERVER_PIPE
			2. dwDesiredAccess: ���� ����� ����ϱ� ���ؼ� ����Ѵ�. �Ϲ������� GENERIC_READ, GENERIC_WRITE Ȥ�� GENERIC_READ|GENERIC_WRITE�� ����Ѵ�. (GENERIC_READ | GENERIC_WRITE)
			3. dwShareMode: ��ü�� ���� ����� �����Ѵ�. 0�� �����ϸ� ������ �� ���� ���°� �ǰ�, �ڵ��� ������ ������ �ٸ� ����� �����ϰ� �ȴ�. 
			4. lpSecurityAttributes: NULL�� ����
			5. dwCreationDisposition: ������ ��������� ����Ѵ�. (OPEN_EXISTING)
			6. dwFlagsAndAttributes: ������ ��Ÿ �Ӽ��� �����Ѵ�. (FILE_ATTRIBUTE_NORMAL)
			7. hTemplateFile: ������ ���Ͽ� ���� �Ӽ��� �����ϴ� ���ø� (NULL)

			return value : �����ϸ� ���Ͽ� ���� �ڵ��� ��ȯ�Ѵ�. �����ϸ� INVALID_HANDLE_VALUE�� ��ȯ�Ѵ�.
			*/
			hClient = CreateFileA(SERVER_PIPE, GENERIC_READ | GENERIC_WRITE, 0, NULL,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

			// ���� ������ �����ϸ� Ŭ���̾�Ʈ �ڵ� ����
			if (hClient != INVALID_HANDLE_VALUE) CloseHandle(hClient);

			WaitForSingleObject(hConTh, INFINITE); // �ٽ� ����Ǳ� ��ٸ�. hConTh�� signaled state���� ������ ���
		}
	}


	printf("Thread %d shutting down.\n", pThArg->threadNumber);
	/* End of command processing loop. Free resources and exit from the thread. */
	CloseHandle(hTmpFile); ; // hTmpFile �ڵ��� ����
	hTmpFile = INVALID_HANDLE_VALUE; ; // hTmpFile �ڵ鿡 INVALID_HANDLE_VALUE �Ҵ���

	if (!DeleteFileA(pThArg->tempFileName)) { // �ӽ����� pThArg->tempFileName ����
		ReportError("Failed deleting temp file.", 0, TRUE); // �ӽ����� ������ �����ϸ� ���� �����
	}

	// ���� �����ϴ� ������ ����� �����
	printf("Exiting server thread number %d.\n", pThArg->threadNumber);
	return 0;
}

// Ŭ���̾�Ʈ�� �������� �����ϴ°�
static DWORD WINAPI Connect(PTHREAD_ARG pThArg)
{
	BOOL fConnect; // connect flag ���� ����

	/* 
	ConnectNamedPipe �Լ� : �������� ���ӵ� �������� �����ϴ� Ŭ���̾�Ʈ ���μ����� ��ٸ���. 
	- Ŭ���̾�Ʈ�� createfile�ϸ� (Ȥ�� CallNamedPipe) Connect���� ConnectNamedPipe�� ������
	- ������ �������� ���� Ŭ���̾�Ʈ�κ��� ���� ��û�� ������ �� ��ٸ��� ������ �Լ�
	1. hNamedPipe : CreteNamedPipe�� ������� ������ �ڵ鷯. ( pThArg->hNamedPipe�� ����)
	2. lpOverlapped: NULL �̸� ����� �������� Ŭ���̾�Ʈ�� ������ �� ������ ��ٸ�
	return value: ���� �Լ��� �����ϸ� 0�� �ƴ� ���� ��ȯ�Ѵ�. �����ϸ� 0�� ��ȯ�Ѵ�. 
	 */
	/*	Pipe connection thread that allows the server worker thread
		to poll the shut down flag. */
	fConnect = ConnectNamedPipe(pThArg->hNamedPipe, NULL);
	_endthreadex(0);  // ������ ����
	return 0;
}

// �ڽ��� ���� ������ �̸��� Ŭ���̾�Ʈ���� �� �� �ֵ���  MailSlot�� �̿��� �������ִ� server broadcast
// Ŭ���̾�Ʈ�� locate ���� ����ϸ鼭 ������ �̸��� ����
static DWORD WINAPI ServerBroadcast(LPLONG pNull)
{
	MS_MESSAGE MsNotify; // MsNotify ����
	DWORD nXfer, iNp; // nXfer, iNp ����
	HANDLE hMsFile; // �ڵ� hMsFile ���� 

	/* Open the mailslot for the MS "client" writer. */
	// shutDown�� 0�ε��� ������ �ݺ���
	while (!shutDown) { /* Run as long as there are server threads */
		/* Wait for another client to open a mailslot. */
		Sleep(CS_TIMEOUT); // // CS_TIMEOUT=5�ʵ��� sleep, 5�ʸ��� mailslot���ٰ� ���� ������ �̸��� �����ϱ� ����
		/* SERVER_PIPE�� ���ؼ� ���� open
		* CreateFileA �Լ�: �����̳� ����� ��ġ�� ����. ��ǥ���� ����� ��ġ�δ� ����, ���� ��Ʈ��, ���丮, �������� ��ũ, ����, �ܼ� ����, ���̺� ����̺�, ������ ���� �ִ�.
		* �� �Լ��� �� ��ġ�� ������ �� �ִ� handle(�ڵ�)�� ��ȯ�Ѵ�.

		1. lpFileName: ������ �ϴ� ���� �̸� (MS_CLTNAME)
		2. dwDesiredAccess: ���� ����� ����ϱ� ���ؼ� ����Ѵ�.GENERIC_WRITE���� ������
		3. dwShareMode: ��ü�� ���� ����� �����Ѵ�. 0�� �����ϸ� ������ �� ���� ���°� �ǰ�, �ڵ��� ������ ������ �ٸ� ����� �����ϰ� �ȴ�. (FILE_SHARE_READ)
		4. lpSecurityAttributes: NULL ������ ������� ����
		5. dwCreationDisposition: ������ ��������� ����Ѵ�. (OPEN_EXISTING: ������ �����ϸ� ����. ���� �����̳� ��ġ�� �������� ������, ���� �ڵ�� ERROR_FILE_NOT_FOUND (2)�� �����Ѵ�.)
		6. dwFlagsAndAttributes: ������ ��Ÿ �Ӽ��� �����Ѵ�. (FILE_ATTRIBUTE_NORMAL: �ٸ� �Ӽ��� ������ �ʴ´�.)
		7. hTemplateFile: ������ ���Ͽ� ���� �Ӽ��� �����ϴ� ���ø� (NULL)

		return value : �����ϸ� ���Ͽ� ���� �ڵ��� ��ȯ�Ѵ�. �����ϸ� INVALID_HANDLE_VALUE�� ��ȯ�Ѵ�.
		*/
		hMsFile = CreateFileA(MS_CLTNAME, GENERIC_WRITE,
			FILE_SHARE_READ,
			NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		// ���� ������ �����ϸ� conitnue (�ٽ� ������)
		if (hMsFile == INVALID_HANDLE_VALUE) {
			continue;
		}

		/* Send out the message to the mailslot. */
		cout << "mailslot�� ���� �޼����� �����ϴ�." << endl;
		MsNotify.msStatus = 0; // MsNotify.msStatus�� 0���� �Ҵ�
		MsNotify.msUtilization = 0; // MsNotify.msUtilization�� 0���� �Ҵ�
		strncpy_s(MsNotify.msName, sizeof(MsNotify.msName) / sizeof(CHAR), SERVER_PIPE, _TRUNCATE); // SERVER_PIPE��MsNotify.msName���� ����

		// WriteFile�� ���� MsNotify �����Ͱ� ���۵Ǵ� ������ MSM_SIZE��ŭ hMsFile�� ä��
		if (!WriteFile(hMsFile, &MsNotify, MSM_SIZE, &nXfer, NULL))
			ReportError("Server MS Write error.", 13, TRUE);
		CloseHandle(hMsFile); // hMsFIle �ڵ� ����
	}

	/* Cancel all outstanding NP I/O commands. See Chapter 14 for CancelIoEx */
	printf("Shut down flag set. Cancel all outstanding I/O operations.\n");
	/* This is an NT6 dependency. On Windows XP, outstanding NP I/O operations hang. */
	for (iNp = 0; iNp < MAX_CLIENTS; iNp++) {
		CancelIoEx(threadArgs[iNp].hNamedPipe, NULL); // CancelIoEx �Լ��� ���ؼ� Ư�� �񵿱� I/O���� ���
	}
	printf("Shuting down monitor thread.\n");

	_endthreadex(0); // ������ ����
	return 0;
}


// �ý����� �˴ٿ� ��
BOOL WINAPI Handler(DWORD CtrlEvent)
{
	// handler �Լ��� ��� �ȵǾ� ������ ctrl+c ���� ��� ����Ǵ� �� ���� control handeler�� ���� shutDown ������ 1 ������Ŵ  (����ȭ)
	printf("In console control handler\n");
	InterlockedIncrement(&shutDown); // ShutDown������ 1 ���� (�ٸ� �����忡�� ���� ���ϵ��� InterlockedIncrement�� ������Ŵ)

	return TRUE;
}
