using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using log4net;
using System.Data;
using System.Data.SQLite;

namespace Short_Messager
{
    //
    // 待发送表的接收短信号码字段内可以存储多个号码，用逗号分隔。每个这样的记录生成多个待发送记录。
    // 短信提交发送后会收到一个回执，是一个编号。此次发送是否成功，还需再次调用另一个接口，输入参数是上次的得到的回执编号。
    // 然后才知道此次发送是否成功。
    // 每个短信发送任务做为一个对象，发送完毕后以返回的编号为键值存储这个对象。查询到这个任务成功失败后从内存中删除，再写
    // 入数据库。
    // 空闲时查询帐号获得余额信息写入数据库。
    class SMSenderThread
    {
        log4net.ILog log = log4net.LogManager.GetLogger("smsender");
        SQLiteConnection confconn = new SQLiteConnection();
        SQLiteConnection realtimeconn = new SQLiteConnection();
        SQLiteConnection historyconn = new SQLiteConnection();
        bool bconfopened = false;
        bool brealtimedbopened = false;
        bool bhistorydbopened = false;
        string smUserName = "jyxx";
        string smPassWord = "jyxx9631";
        List<WaitingSendSM> WaitingSendSMList = new List<WaitingSendSM>();
        List<WaitingSendSM> WaitingSendSMListTmp = new List<WaitingSendSM>();
        List<Int32> recIDList = new List<int>();
        Dictionary<string, WaitingSendSM> waitingResultSMDict_ = new Dictionary<string, WaitingSendSM>();
        TopenServiceReference.MsgSendSoapClient topen = new TopenServiceReference.MsgSendSoapClient();
        // This method will be called when the thread is started.
        public void DoWork()
        {

            log.Info("running");

            //查询表发送短信。
            //调用短信服务接口检查发送情况。
            //检查短信帐户，写数据库。
            while (true)
            {
                #region OPEN SQLITE3 DB FILE
                try
                {
                    if (bconfopened == false)
                    {//生产环境下使用相对路径。
                        confconn.ConnectionString = @"Data Source=c:\\sqlitedbs\\config.db";
                        confconn.Open(); bconfopened = true;
                    }
                }
                catch { bconfopened = false; log.Info("open config db error."); }
                try
                {
                    if ( brealtimedbopened == false )
                    {
                        realtimeconn.ConnectionString = @"Data Source=c:\\sqlitedbs\\realtime.db";
                        realtimeconn.Open(); brealtimedbopened = true;
                    }
                }
                catch {brealtimedbopened = false; log.Info("open realtime db error.");}
                try
                {
                    if ( bhistorydbopened == false )
                    {
                        historyconn.ConnectionString = @"Data Source=c:\\sqlitedbs\\history.db";
                        historyconn.Open(); bhistorydbopened = true;
                    }
                }
                catch {bhistorydbopened = false; log.Info("open history db error.");}
                if (brealtimedbopened == false || bhistorydbopened == false || bconfopened == false)
                    continue;//任意一个数据库如无法打开不允许执行核心逻辑
                #endregion

                #region GET SHORT MESSAGE CONFIGURATION FROM DB
                StringBuilder smConfStrSql = new StringBuilder();
                smConfStrSql.Append("SELECT * FROM shortmessageinfo");
                DataSet smConfds = SQLiteHelper.Query(confconn, smConfStrSql.ToString());
                DataTable smConftbl = smConfds.Tables[0];
                foreach (DataRow row in smConftbl.Rows)
                {
                    smUserName = row["username"].ToString();
                    smPassWord = row["pass"].ToString();
                    break;
                }
                #endregion


                #region GET WAITING TO SEND SHORT MESSAGE FROM WaitingSendSM TABLE
                StringBuilder strSql = new StringBuilder();
                strSql.Append("SELECT * FROM waitingsendsm");
                DataSet ds = SQLiteHelper.Query(realtimeconn, strSql.ToString());
                DataTable tbl = ds.Tables[0];
                recIDList.Clear();
                foreach (DataRow row in tbl.Rows)
                {
                    string rowstr;
                    rowstr = String.Format("mobile no:{0}  message:{1}  schedule time:{2}  type:{3}  to id:{4}  to name:{5}  ani:{6}  dnis:{7}  from id:{8}  from name:{9}\r\n", row["mobileno"], row["smmessage"], row["schedulesendtime"], row["smtype"], row["touserid"], row["tousername"], row["ani"], row["dnis"], row["fromuserid"], row["fromusername"]);
                    log.Info(rowstr);
                    string mobilenos = row["mobileno"].ToString();
                    string[] mobilenoarray = mobilenos.Split(',');
                    foreach (string mobileno in mobilenoarray)
                    {
                        WaitingSendSM waitingSendSM = new WaitingSendSM();
                        recIDList.Add(Int32.Parse(row["id"].ToString()));
                        waitingSendSM.id_ = Int32.Parse(row["id"].ToString());
                        waitingSendSM.mobileno_ = mobileno;
                        waitingSendSM.smmessage_ = row["smmessage"].ToString();
                        waitingSendSM.schedulesendtime_ = row["schedulesendtime"].ToString();
                        waitingSendSM.smtype_ = row["smtype"].ToString();
                        waitingSendSM.tosuerid_ = Int32.Parse(row["touserid"].ToString());
                        waitingSendSM.tosuername_ = row["tousername"].ToString();
                        waitingSendSM.ani_ = row["ani"].ToString();
                        waitingSendSM.dnis_ = row["dnis"].ToString();
                        waitingSendSM.fromsuerid_ = Int32.Parse(row["fromuserid"].ToString());
                        waitingSendSM.fromsuername_ = row["fromusername"].ToString();
                        WaitingSendSMList.Add(waitingSendSM);
                    }
                }
                if (recIDList.Count > 0)
                {
                    log.Info(String.Join(",", recIDList.ToArray()));
                    recIDList.Clear();
                }
                //待发送记录读出后数据库内不立即删除，待发送成功后或者达到重试次数限制再从数据库内删除。
                #endregion

                #region SEND SHORT MESSAGE FROM WAITING TO SEND LIST
                bool bCanDelFromWaiting = false;
                WaitingSendSMListTmp.Clear();
                WaitingSendSMListTmp = WaitingSendSMList;
                WaitingSendSMList.Clear();
                foreach (WaitingSendSM sendSM in WaitingSendSMListTmp)
                {
                    string mobiles = sendSM.mobileno_;
                    string msgContent = "用户号码*139请您回电，*20140815【上海巨优】";
                    string channel = "0";
                    string sendResult = topen.SendMsg(smUserName, smUserName, mobiles, msgContent, channel);
                    log.Info(String.Format("SendMsg return :{0}    {1}", sendResult, GetSMReturnCodeString(sendResult)));
                    if (sendResult[0] == '-')
                    {
                        if (sendResult[0] == '-' && (sendResult[1] == '3' || sendResult[1] == '7' || sendResult[1] == '8' || sendResult[1] == '9'))
                        {
                            ++(sendSM.sendtimes_);
                            if (sendSM.sendtimes_ > 3)
                                bCanDelFromWaiting = true;
                            else
                            {
                                //可以再次重发
                                WaitingSendSMList.Add(sendSM);
                            }
                        }
                    }
                    else
                    {
                        bCanDelFromWaiting = false;
                        waitingResultSMDict_.Add(sendResult, sendSM);
                    }

                    if (bCanDelFromWaiting == true)
                    {
                        string strDeleteSql = String.Format("DELETE FROM waitingsendsm WHERE ID = {0}", sendSM.id_);
                        int recs = 0;
                        try
                        {
                            recs = SQLiteHelper.ExecuteSql(realtimeconn, strDeleteSql);
                        }
                        catch (Exception e)
                        {
                            log.Info(String.Format("Delete record fatal. error:{0}", e.Message));
                        }
                        if (recs >= 1)
                            log.Info(String.Format("Delete record from waitingsendsm suc. id:{0}", sendSM.id_));
                        else
                            log.Info(String.Format("Delete record from waitingsendsm fail. id:{0}", sendSM.id_));
                    }
                }
                #endregion

                #region CHECK SEND SHORT MESSAGE RESULT
                string getReport2Result = topen.GetReport2(smUserName, smUserName);
                log.Info(String.Format("GetReport2 return :{0}", getReport2Result));
                if (!string.IsNullOrEmpty(getReport2Result.Trim()))
                {
                    string[] resultArray = getReport2Result.Trim().Split(new Char[]{'|'});
                    foreach (string sendResult in resultArray)
                    {
                        string[] sendRecords = sendResult.Trim().Split(new Char[]{','});
                        string batchNo = sendRecords[0];
                        string phoneNo = sendRecords[1];
                        string timeStr = sendRecords[2];
                        string result = sendRecords[3];
                    }
                }
                #endregion

                Thread.Sleep(500000);
            }
        }

        string GetSMReturnCodeString(string p_returncode)
        {
            switch (p_returncode)
            {
                case "-1":
                    return "提交接口错误";
                case "-3":
                    return "用户名或密码错误";
                case "-4":
                    return "短信内容和备案的模板不一样";
                case "-5":
                    return "签名不正确签名一定要放在短信最后";
                case "-7":
                    return "余额不足";
                case "-8":
                    return "通道错误";
                case "-9":
                    return "无效号码";
                default:
                    return "Unknown return code";
            }
        }
    }
}
/*TopenServiceReference.MsgSendSoapClient topen = new TopenServiceReference.MsgSendSoapClient();
string userName = "jyxx";
string passWord = "jyxx9631";
string mobiles = "13916394304";
string msgContent = "用户号码*139163请您回电，*20140815【上海巨优】";
string channel = "0";
string sendResult = topen.SendMsg(userName, passWord, mobiles, msgContent, channel);
log.Info(sendResult);

sendResult = topen.GetReport(userName, passWord, "100");
sendResult = topen.GetBalance(userName, passWord);*/