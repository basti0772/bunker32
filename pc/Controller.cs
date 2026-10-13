using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
using System.IO.Ports;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace Bunker32PC {
    sealed class ScreenPanel : Panel {
        public Bitmap Frame;
        public ScreenPanel() { DoubleBuffered=true; BackColor=Color.FromArgb(9,14,19); }
        protected override void OnPaint(PaintEventArgs e) {
            base.OnPaint(e);
            if(Frame!=null) {
                e.Graphics.InterpolationMode=InterpolationMode.NearestNeighbor;
                e.Graphics.PixelOffsetMode=PixelOffsetMode.Half;
                e.Graphics.DrawImage(Frame,new Rectangle(0,0,Width,Height));
            } else {
                using(var font=new Font("Segoe UI",15)) using(var brush=new SolidBrush(Color.FromArgb(129,159,170)))
                    e.Graphics.DrawString("Warte auf den ESP32 ...",font,brush,30,Height/2-20);
            }
        }
        protected override void Dispose(bool disposing) { if(disposing&&Frame!=null)Frame.Dispose();base.Dispose(disposing); }
    }
    public sealed class Controller : Form, IMessageFilter {
        readonly ScreenPanel screen=new ScreenPanel();
        readonly Label status=new Label(), stats=new Label();
        readonly ComboBox ports=new ComboBox();
        readonly Timer timer=new Timer();
        readonly HashSet<Keys> held=new HashSet<Keys>();
        readonly List<byte> received=new List<byte>();
        readonly byte[] scratch=new byte[65536];
        SerialPort serial;
        DateTime lastFrame=DateTime.MinValue,lastReconnect=DateTime.MinValue;
        string selectedPort;
        int frames=0;
        bool closing=false,connecting=false;
        readonly string capturePath;
        public Controller(string port,string capture) {
            selectedPort=port;capturePath=capture;
            Text="Bunker 32 - ESP32 spielen";ClientSize=new Size(950,650);
            FormBorderStyle=FormBorderStyle.FixedSingle;MaximizeBox=false;StartPosition=FormStartPosition.CenterScreen;
            BackColor=Color.FromArgb(14,20,26);ForeColor=Color.FromArgb(226,230,229);
            Font=new Font("Segoe UI",10);
            Label title=LabelAt("BUNKER 32",24,17,450,40);title.Font=new Font("Segoe UI",24,FontStyle.Bold);title.ForeColor=Color.FromArgb(239,179,71);
            LabelAt("RETRO-SHOOTER / ESP32 + USB",26,60,600,23).ForeColor=Color.FromArgb(117,153,162);
            screen.SetBounds(24,99,640,480);Controls.Add(screen);
            LabelAt("TASTATUR",693,99,225,30).Font=new Font("Segoe UI",13,FontStyle.Bold);
            LabelAt("W / S     Vor / zurück\nA / D     Links / rechts drehen\nQ / E     Seitwärts gehen\nLeertaste  Feuern\n\nEnter      Start / weiter\nP / Esc   Pause\nM            Karte\nF             Auto-Feuer an / aus\nR             Neue Runde",693,141,237,260);
            ButtonAt("Start / weiter",693,412,225,35,delegate{Send("START");});
            ButtonAt("Pause / weiter",693,454,109,34,delegate{Send("PAUSE");});
            ButtonAt("Karte",809,454,109,34,delegate{Send("MAP");});
            ButtonAt("Auto-Feuer",693,495,109,34,delegate{Send("AUTO");});
            ButtonAt("Touch kalibr.",809,495,109,34,delegate{Send("CAL");});
            ButtonAt("Kalibr. abbrechen",693,536,225,34,delegate{Send("CANCEL");});
            status.SetBounds(24,592,640,22);status.ForeColor=Color.FromArgb(129,192,180);Controls.Add(status);
            stats.SetBounds(24,616,880,24);stats.ForeColor=Color.FromArgb(133,149,161);Controls.Add(stats);
            ports.SetBounds(693,591,102,28);ports.DropDownStyle=ComboBoxStyle.DropDownList;Controls.Add(ports);
            RefreshPorts();ButtonAt("Verbinden",803,588,115,31,delegate{selectedPort=Convert.ToString(ports.SelectedItem);Connect();});
            Deactivate+=delegate{held.Clear();Send("K 0");};
            FormClosing+=delegate{closing=true;held.Clear();Send("K 0");Send("STREAM 0");timer.Stop();CloseSerial();Application.RemoveMessageFilter(this);};
            Shown+=delegate{Connect();};
            Application.AddMessageFilter(this);timer.Interval=33;timer.Tick+=Tick;timer.Start();
        }
        Label LabelAt(string text,int x,int y,int w,int h) {
            var label=new Label{Text=text,AutoSize=false};label.SetBounds(x,y,w,h);Controls.Add(label);return label;
        }
        void ButtonAt(string text,int x,int y,int w,int h,Action action) {
            var b=new Button{Text=text,FlatStyle=FlatStyle.Flat,BackColor=Color.FromArgb(31,45,54),ForeColor=Color.FromArgb(232,229,216),TabStop=false};
            b.FlatAppearance.BorderColor=Color.FromArgb(61,80,88);b.SetBounds(x,y,w,h);b.Click+=delegate{action();screen.Focus();};Controls.Add(b);
        }
        void RefreshPorts() {
            ports.Items.Clear();foreach(string p in SerialPort.GetPortNames())ports.Items.Add(p);
            if(ports.Items.Contains(selectedPort))ports.SelectedItem=selectedPort;
            else if(ports.Items.Count==1){ports.SelectedIndex=0;selectedPort=Convert.ToString(ports.SelectedItem);}
        }
        void CloseSerial() { if(serial!=null){try{serial.Close();serial.Dispose();}catch{}serial=null;} }
        void Connect() {
            if(connecting||closing)return;connecting=true;
            try {
                CloseSerial();received.Clear();held.Clear();lastReconnect=DateTime.UtcNow;
                if(String.IsNullOrWhiteSpace(selectedPort))throw new IOException("COM-Port auswählen.");
                serial=new SerialPort(selectedPort,921600,Parity.None,8,StopBits.One);
                serial.DtrEnable=false;serial.RtsEnable=false;serial.ReadTimeout=20;serial.WriteTimeout=100;
                serial.ReadBufferSize=131072;serial.Open();
                Send("K 0");Send("STREAM 1");status.Text=selectedPort+" verbunden - Enter startet die Mission";lastFrame=DateTime.UtcNow;
            } catch(Exception ex){status.Text="Verbindung: "+ex.Message;CloseSerial();}
            finally{connecting=false;}
        }
        void Send(string command) {
            if(serial==null||!serial.IsOpen)return;
            try{serial.Write(command+"\n");}catch(Exception ex){status.Text="USB getrennt: "+ex.Message;CloseSerial();held.Clear();}
        }
        public bool PreFilterMessage(ref Message m) {
            if(Form.ActiveForm!=this)return false;
            if(m.Msg!=0x100&&m.Msg!=0x101&&m.Msg!=0x104&&m.Msg!=0x105)return false;
            Keys key=(Keys)m.WParam.ToInt32();
            bool relevant=key==Keys.W||key==Keys.S||key==Keys.A||key==Keys.D||key==Keys.Q||key==Keys.E||key==Keys.Up||key==Keys.Down||key==Keys.Left||key==Keys.Right||key==Keys.Space||key==Keys.Enter||key==Keys.P||key==Keys.Escape||key==Keys.M||key==Keys.F||key==Keys.R;
            if(!relevant)return false;
            if(m.Msg==0x101||m.Msg==0x105){held.Remove(key);return true;}
            if(!held.Add(key))return true;
            switch(key){case Keys.Enter:Send("START");break;case Keys.P:case Keys.Escape:Send("PAUSE");break;case Keys.M:Send("MAP");break;case Keys.F:Send("AUTO");break;case Keys.R:Send("NEW");break;}
            return true;
        }
        int InputMask() {
            int v=0;if(held.Contains(Keys.W)||held.Contains(Keys.Up))v|=1;if(held.Contains(Keys.S)||held.Contains(Keys.Down))v|=2;
            if(held.Contains(Keys.A)||held.Contains(Keys.Left))v|=4;if(held.Contains(Keys.D)||held.Contains(Keys.Right))v|=8;
            if(held.Contains(Keys.Q))v|=16;if(held.Contains(Keys.E))v|=32;if(held.Contains(Keys.Space))v|=64;return v;
        }
        void Tick(object sender,EventArgs e) {
            if(serial==null){if((DateTime.UtcNow-lastReconnect).TotalSeconds>3){RefreshPorts();Connect();}return;}
            Send("K "+InputMask());if(serial==null)return;
            try {
                int n=Math.Min(scratch.Length,serial.BytesToRead);
                if(n>0){n=serial.Read(scratch,0,n);for(int i=0;i<n;i++)received.Add(scratch[i]);}
                ParseFrames();
                if((DateTime.UtcNow-lastFrame).TotalSeconds>3){Send("STREAM 1");lastFrame=DateTime.UtcNow;status.Text="Warte auf Bilddaten von "+selectedPort+" ...";}
            }catch(Exception ex){status.Text="USB: "+ex.Message;CloseSerial();held.Clear();}
        }
        void ParseFrames() {
            while(received.Count>=8) {
                int at=0;while(at+3<received.Count&&!(received[at]==66&&received[at+1]==51&&received[at+2]==50&&received[at+3]==70))at++;
                if(at>0)received.RemoveRange(0,at);
                if(received.Count<8)return;
                int size=received[4]|received[5]<<8|received[6]<<16|received[7]<<24;
                if(size!=38400){received.RemoveAt(0);continue;}
                if(received.Count<16+size)return;
                byte[] packet=received.GetRange(0,16+size).ToArray();received.RemoveRange(0,16+size);
                var bmp=new Bitmap(160,120,PixelFormat.Format24bppRgb);
                var data=bmp.LockBits(new Rectangle(0,0,160,120),ImageLockMode.WriteOnly,PixelFormat.Format24bppRgb);
                byte[] rgb=new byte[data.Stride*120];
                for(int y=0;y<120;y++)for(int x=0;x<160;x++){
                    int k=16+(y*160+x)*2,c=packet[k]|packet[k+1]<<8,p=y*data.Stride+x*3;
                    rgb[p]=(byte)((c&31)*255/31);rgb[p+1]=(byte)(((c>>5)&63)*255/63);rgb[p+2]=(byte)(((c>>11)&31)*255/31);
                }
                Marshal.Copy(rgb,0,data.Scan0,rgb.Length);bmp.UnlockBits(data);
                var old=screen.Frame;screen.Frame=bmp;screen.Invalidate();if(old!=null)old.Dispose();
                string[] modes={"Bereit - Enter startet","Mission läuft","Pause","Entkommen! Enter: neue Runde","Game Over - Enter: neue Runde"};
                status.Text=selectedPort+"  |  "+modes[Math.Min(4,(int)packet[11])]+"  |  ESP32 "+packet[12]+" FPS";
                stats.Text="HP "+packet[8]+"   Munition "+packet[9]+"   Wachen "+packet[10]+"/9   Schlüssel "+(packet[13]!=0?"JA":"NEIN")+"   Auto-Feuer "+(packet[14]!=0?"AN":"AUS")+"     USB-Vorschau ~2 Bilder/s; direkt am Display flüssiger.";
                lastFrame=DateTime.UtcNow;frames++;
                if(capturePath!=null&&frames==2)bmp.Save(capturePath,ImageFormat.Png);
            }
        }
        [STAThread]
        public static void Main(string[] args) {
            string port="COM3",capture=null;
            for(int i=0;i<args.Length;i++){if(args[i]=="--port"&&i+1<args.Length)port=args[++i];else if(args[i]=="--capture"&&i+1<args.Length)capture=args[++i];}
            Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);Application.Run(new Controller(port,capture));
        }
    }
}
