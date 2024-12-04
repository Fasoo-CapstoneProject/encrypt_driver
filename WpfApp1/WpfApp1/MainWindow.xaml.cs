using System;
using System.IO;
using System.Linq; // For LINQ methods
using System.Net.Sockets;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Forms;

namespace AgentApp
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
        }

        private void BrowseButton_Click(object sender, RoutedEventArgs e)
        {
            if (FileRadioButton.IsChecked == true)
            {
                // 파일 선택
                var openFileDialog = new OpenFileDialog
                {
                    Filter = "All Files (*.*)|*.*",
                    Title = "Select a File"
                };

                if (openFileDialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
                {
                    FilePathTextBox.Text = openFileDialog.FileName;
                }
            }
            else if (DirectoryRadioButton.IsChecked == true)
            {
                // 디렉토리 선택
                var folderBrowserDialog = new FolderBrowserDialog
                {
                    Description = "Select a Directory"
                };

                if (folderBrowserDialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
                {
                    FilePathTextBox.Text = folderBrowserDialog.SelectedPath;
                }
            }
        }

        private void SendButton_Click(object sender, RoutedEventArgs e)
        {
            string filePath = FilePathTextBox.Text;
            string selectedExtension = (FileExtensionComboBox.SelectedItem as ComboBoxItem)?.Content.ToString();

            if (string.IsNullOrEmpty(filePath))
            {
                StatusTextBlock.Text = "Please select a file or directory path.";
                return;
            }

            try
            {
                if (Directory.Exists(filePath))
                {
                    // 파일 패턴 생성 (예: C:\path\*.txt)
                    string filePattern = Path.Combine(filePath, $"*{selectedExtension}");
                    
                    // 파일 패턴을 서버로 전송
                    SendDataToServer(filePattern);
                    StatusTextBlock.Text = "Directory file pattern sent successfully.";
                }
                else if (File.Exists(filePath))
                {
                    // 파일 경로 전송
                    if (!filePath.EndsWith(selectedExtension, StringComparison.OrdinalIgnoreCase))
                    {
                        StatusTextBlock.Text = "File extension does not match the selected extension.";
                        return;
                    }

                    SendDataToServer(filePath);
                    StatusTextBlock.Text = "File path sent successfully.";
                }
                else
                {
                    try
                    {
                        // 파일 패턴 생성 (예: C:\path\*.txt)
                        string filePattern = Path.Combine(filePath, $"*{selectedExtension}");
                        
                        // 파일 패턴을 서버로 전송
                        SendDataToServer(filePattern);
                        StatusTextBlock.Text = "File pattern sent successfully.";
                    }
                    catch (Exception ex)
                    {
                        StatusTextBlock.Text = $"Error: {ex.Message}";
                    }
                }
            }
            catch (Exception ex)
            {
                StatusTextBlock.Text = $"Error: {ex.Message}";
            }
        }

        private void SendDataToServer(string data)
        {
            const string serverAddress = "127.0.0.1";
            const int serverPort = 8080;

            using (TcpClient client = new TcpClient(serverAddress, serverPort))
            using (NetworkStream stream = client.GetStream())
            {
                byte[] message = Encoding.UTF8.GetBytes(data);
                stream.Write(message, 0, message.Length);

                byte[] buffer = new byte[1024];
                int bytesRead = stream.Read(buffer, 0, buffer.Length);
                string response = Encoding.UTF8.GetString(buffer, 0, bytesRead);

                System.Windows.MessageBox.Show($"Server Response: {response}", "Response");
            }
        }
    }
}