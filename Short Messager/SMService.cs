using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Linq;
using System.ServiceProcess;
using System.Text;
using System.Configuration;
using System.Threading;
using log4net;

namespace Short_Messager
{
    public partial class SMService : ServiceBase
    {
        log4net.ILog log = log4net.LogManager.GetLogger("root");
        SMSenderThread smsender;
        Thread smsenderThread_;

        public SMService()
        {
            InitializeComponent();
        }

        protected override void OnStart(string[] args)
        {
            log.Info("Starting...");
            smsender = new SMSenderThread();
            smsenderThread_ = new Thread(smsender.DoWork);
            smsenderThread_.Start();
        }

        protected override void OnStop()
        {
            smsenderThread_.Abort();
            smsenderThread_.Join();
        }
    }
}
