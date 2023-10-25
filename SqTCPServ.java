import java.io.*;
import java.net.*;

public class SqTCPServ {

    private static final int PORT = 5000;

    private ServerSocket serverSocket;

    public SqTCPServ() {
        try {
            System.out.println("Listening on port " + PORT);
            serverSocket = new ServerSocket(PORT);

            while (true) {
                Socket socket = serverSocket.accept();
                System.out.println("Connection accepted from: " + socket);

                Thread t = new ClientHandler(socket);
                t.start();
            }
        } catch (IOException e) {
            System.err.println("I/O error: " + e.getMessage());
        }
    }

    public static void main(String[] args) {
        new SqTCPServ();
    }

    private static class ClientHandler extends Thread {
        private final Socket socket;

        public ClientHandler(Socket socket) {
            this.socket = socket;
        }

        public void run() {
            try {
                BufferedWriter w = new BufferedWriter(new OutputStreamWriter(socket.getOutputStream()));
                BufferedReader r = new BufferedReader(new InputStreamReader(socket.getInputStream()));

                char[] buf = new char[100];
                String input = new String(buf, 0, r.read(buf, 0, buf.length));
                System.out.println("Got: " + input);

                try {
                    int i = Integer.parseInt(input);
                    i = i * i;
                    w.write(String.valueOf(i));
                    w.newLine();
                    w.flush();
                    System.out.println("Sent: " + i);
                } catch (NumberFormatException e) {
                    w.write("Invalid input, please send an integer.");
                    w.newLine();
                    w.flush();
                    System.err.println("Not an integer");
                }

                socket.close();
            } catch (IOException e) {
                System.err.println("I/O error: " + e.getMessage());
            }
        }
    }
}
