using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Nefarius.ViGEm.Client;
using Nefarius.ViGEm.Client.Targets;
using Nefarius.ViGEm.Client.Targets.Xbox360;
using Nefarius.ViGEm.Client.Targets.DualShock4;
using System.Collections.ObjectModel;
using System.Collections.Specialized;
using System.Threading;
using System.Net;
using System.Net.Sockets;
using System.Configuration;

namespace X3DS
{
    struct CirclePosition
    {
        public Int16 dx;
        public Int16 dy;
    }

    struct TouchPosition
    {
        public UInt16 px;
        public UInt16 py;
    }

    struct InputStatus
    {
        public UInt32 kDown;
        public UInt32 kHeld;
        public UInt32 kUp;
        public CirclePosition pad;
        public CirclePosition cstick;
        public TouchPosition touch;
    }

    

    class Program
    {
        static Xbox360Buttons?[] X3DSMap = new Xbox360Buttons?[] {
            Xbox360Buttons.A,
            Xbox360Buttons.B,
            Xbox360Buttons.Back,
            Xbox360Buttons.Start,
            Xbox360Buttons.Right,
            Xbox360Buttons.Left,
            Xbox360Buttons.Up,
            Xbox360Buttons.Down,
            Xbox360Buttons.RightShoulder,
            Xbox360Buttons.LeftShoulder,
            Xbox360Buttons.X,
            Xbox360Buttons.Y,
            null, null,
            null, //zl
            null, //zr
            null, null, null, null,
            null, //touch
            null, null, null, null, //cstick right left up down
            null, null, null, null, //pad
            null, null, null, null, null
        };

        static void Main(string[] args)
        {
            var client = new ViGEmClient();
            var x360 = new Xbox360Controller(client);
            
            IPEndPoint ipep = new IPEndPoint(IPAddress.Parse(ConfigurationManager.AppSettings["bindAddr"]), UInt16.Parse(ConfigurationManager.AppSettings["bindPort"]));
            UdpClient newsock = new UdpClient(ipep);
            IPEndPoint senderep = new IPEndPoint(IPAddress.Any, 0);

            x360.FeedbackReceived +=
                (sender, eventArgs) =>
                {

                };

            x360.Connect();
            Console.WriteLine("Controller connected.");

            Console.WriteLine("Listening on {0}", ipep);

            var report = new Xbox360Report();
            var lastRecv = DateTime.Now;
            float tt = 0;
            int second_counter = 0;
            while (true)
            {
                var dt_t = DateTime.Now - lastRecv;
                lastRecv = DateTime.Now;
                float dt = dt_t.Milliseconds / 1000.0f;
                InputStatus status;
                unsafe
                {
                    fixed(byte *data = &newsock.Receive(ref senderep)[0])
                    {
                        InputStatus* s = (InputStatus*)data;
                        status = *s;
                    }
                }
                report.Buttons = 0;
                for (int i=0; i<32; ++i)
                {
                    if ((status.kHeld & (1 << i)) != 0)
                    {
                        Xbox360Buttons? button = X3DSMap[i];
                        if (button != null)
                        {
                            report.SetButtons(button.Value);
                        }
                    }
                }
                if (status.touch.px > 0 && status.touch.py > 0)
                {
                    if (status.touch.px < 320 / 3)
                    {
                        report.SetButtons(Xbox360Buttons.LeftThumb);
                    }
                    else if (status.touch.px > 320 / 3 * 2)
                    {
                        report.SetButtons(Xbox360Buttons.RightThumb);
                    }
                    else
                    {
                        report.SetButtons(Xbox360Buttons.LeftThumb, Xbox360Buttons.RightThumb);
                    }
                }
                report.SetAxis(Xbox360Axes.LeftThumbX, (short)((float)status.pad.dx * Int16.MaxValue / 160.0f));
                report.SetAxis(Xbox360Axes.LeftThumbY, (short)((float)status.pad.dy * Int16.MaxValue / 160.0f));
                report.SetAxis(Xbox360Axes.RightThumbX, (short)((float)status.cstick.dx * Int16.MaxValue / 160.0f));
                report.SetAxis(Xbox360Axes.RightThumbY, (short)((float)status.cstick.dy * Int16.MaxValue / 160.0f));
                if ((status.kHeld & (1 << 14)) != 0)
                {
                    report.SetAxis(Xbox360Axes.LeftTrigger, Int16.MaxValue);
                }
                else
                {
                    report.SetAxis(Xbox360Axes.LeftTrigger, Int16.MinValue);
                }
                if ((status.kHeld & (1 << 15)) != 0)
                {
                    report.SetAxis(Xbox360Axes.RightTrigger, Int16.MaxValue);
                }
                else
                {
                    report.SetAxis(Xbox360Axes.RightTrigger, Int16.MinValue);
                }
                x360.SendReport(report);

                second_counter++;
                tt += dt;
                if (tt > 1.0f)
                {
                    Console.WriteLine("{0} packet received", second_counter);
                    tt = 0;
                    second_counter = 0;
                }
            }
            Console.ReadKey();
        }
    }
}
