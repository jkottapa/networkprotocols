import java.io.*;
import java.io.File;
import java.io.FileWriter;
import java.net.InetAddress;
import java.net.Socket;
import java.net.ServerSocket;

public class gbnReceiver {

	private static void setSocketInfo() {
		try{
			File recvInfo = new File ("recvInfo");
			
			if( !recvInfo.exists() ) {
				recvInfo.createNewFile();				
			}//if
			
			InetAddress addr = InetAddress.getLocalHost();
			String hostName = addr.getHostName();
			ServerSocket socket = new ServerSocket(0);
			int portNumber = socket.getLocalPort();
			
			FileWriter fw = new FileWriter( recvInfo.getAbsoluteFile() );
			BufferedWriter recvInfoBuff = new BufferedWriter( fw );
			
			recvInfoBuff.write( hostName + " " + portNumber);
			recvInfoBuff.close();
		} catch ( IOException e ) {
			System.err.println("CANNOT WRITE TO FILE: RECEIVER");
		}
	}
	public static void main(String[] args) {
		if(args.length != 1) {
			System.err.println("INVALID COMMANDLINE ARG");
		}
		try {
			File recvData = new File (args[0]);
			
			if( !recvData.exists() ) {			
				recvData.createNewFile();			
			}//if
			
			FileWriter fWrite = new FileWriter( recvData.getAbsoluteFile() );
			BufferedWriter fileWriteRecvData = new BufferedWriter( fWrite );
			
			setSocketInfo();	// set socket host name and port
			
			fileWriteRecvData.close();
		}
		catch (IOException e) {
			System.err.println("CANNOT WRITE TO FILE: RECEIVER");
		}
		

	}

}
