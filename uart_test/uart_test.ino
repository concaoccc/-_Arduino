const int frame_len = 255;
// RSSI数据
byte primary_rssi_data[frame_len*3] = {0};
int rssi_data[frame_len] = {0};

// 交织的数据
int interleave_data[255] = {0};
int interleave_data_len = 0; 

// 平滑的数据
double smooth_data[255] = {0.0};
int smooth_len = 0;

//rank的数据
double rank_data[255] = {0.0};
int rank_len = 0;

// 量化的数据
int q_data[255] ={0};
int q_data_length = 0;
int delete_index[255] = {0};
int delete_index_length = 0; 

//密钥的数据
int key[255] = {0};
int key_len =0;

// 接受缓存区
int uart_rece_buf[255] = {0};
int receive_index_len = 0;

// 保存纠错之后的密钥
int final_key[255] = {0};
int final_len = 0;
int encode_data[] = {0};
int encode_len = 0;

// 最后的密钥数据
//int byte_key[32] = {0};
int byte_key_len = 0;
// 
//获得原始的RSSI
void get_rssi()
{
  int rece_num = 0;
  while(rece_num <frame_len*3 )
  {
    while(Serial1.available() == 0);
    primary_rssi_data[rece_num] = Serial1.read();
    //Serial.print(primary_rssi_data[rece_num]);
    rece_num++;
    }
   // 进行数据转化
   for(int i =0; i<frame_len; i++ )
   {
    // 48是0的ascci码表值
    rssi_data[i] = primary_rssi_data[(i*3)+2] - 48;
    rssi_data[i] += (primary_rssi_data[(i*3)+1] - 48)*10;
    rssi_data[i] = -rssi_data[i];
    }
}

// 一些串口函数的包装
void send_data(int data[], int data_len)
{
  Serial1.write(byte(data_len));
  for (int i=0; i< data_len; i++)
  {
    Serial1.write(byte(data[i]));
    }
  }

void send_data(byte data[], int data_len)
{
  Serial1.write(byte(data_len));
  for (int i=0; i< data_len; i++)
  {
    Serial1.write(data[i]);
    }
  }

int receive()
{
   while(Serial1.available() == 0);
   int receive_len = Serial1.read();
   for (int i=0; i< receive_len; i++)
   {
    while(Serial1.available() == 0);
    uart_rece_buf[i] = Serial1.read();
    }
    return receive_len;
  }

void send_end()
{
  int data[2]={1,1};
  send_data(data,2);
  }
bool get_ack()
{
  while(Serial1.available() == 0);
   int ack = Serial1.read();
   if (ack == 1)
   {
    return true;
    }
    else
    {
      return false;
      }
  }
//进行数据输出
void print_data(int data[], int data_length)
{
  for(int i =0; i<data_length; i++)
  {
    Serial.print(data[i]);
    Serial.print(", ");
    }
    Serial.print("\n");
 }
 void print_data(double data[], int data_length)
{
  for(int i =0; i<data_length; i++)
  {
    Serial.print(data[i]);
    Serial.print(", ");
    }
    Serial.print("\n");
 }
 void print_data(byte data[], int data_length)
{
  for(int i =0; i<data_length; i++)
  {
    Serial.print(data[i]);
    Serial.print(", ");
    }
    Serial.print("\n");
 }
 //进行交织
void interleave()
{
    int m = 20;
    int remainder = frame_len % m;
    interleave_data_len = frame_len;
    int quo = int(frame_len / m);
    int index = 0;
    for(int i =0; i < m; i++)
    {
        if (i < remainder)
        {
            for (int j=0; j< quo+1; j++)
            {
                interleave_data[index] = rssi_data[i+j*m];
                index++;
            }
        }
        
        else
        {
            for (int j =0; j < quo; j++)
            {
                interleave_data[index] = rssi_data[i+j*m];
                index++;
            }
        }
        
    }

}
// 平滑返回的参数是smooth的长度
void smooth()
{
    smooth_len = 0;
    int smooth_order = 7;
    //进行平滑操作
    int n = 0;
    if (smooth_order % 2 ==1)
    {
        n = (smooth_order-1)/2;
    }
    else{
        n = (smooth_order-2)/2;
    }
    for(int i=0; i < interleave_data_len; i++)
    {
        int pn = n;
        if (i < pn)
        {
            pn = i;
        }
        else if ((interleave_data_len - 1 -i)<n)
        {
            /* code */
            pn = interleave_data_len - i - 1;
        }
        double data_sum = 0.0;
        for(int j = int(i-pn); j<int(i+pn)+1; j++)
        {
            data_sum += interleave_data[j];
        }
        smooth_data[smooth_len]  =  int((data_sum/(2*pn+1))*10000)/10000;
        smooth_len++;
    }
  }

// 进行rank操作
// 对smoth的数据进行操作，保存在rank里面
void rank()
{
    double tmp_sum = 1.0;
    int data_length = smooth_len;
    rank_len = smooth_len;
    int K = 10;
    for (int i =0; i < int(data_length/K); i++)
    {
        for(int j =0; j < K; j++)
        {
            for(int s =0; s < K; s++)
            {
                if (i*K+j != i*K+s)
                {
                    if (smooth_data[i*K+j] > smooth_data[i*K+s])
                    {
                        tmp_sum += 1.0;
                    }
                    if (abs(smooth_data[i*K+j] - smooth_data[i*K+s]) == 0)
                    {
                        tmp_sum += 0.5;
                    }
                }
            }
            rank_data[i*K+j] = tmp_sum;
            tmp_sum = 1.0;
        }
    }
}

void double_q()
{
  //对rank_data进行操作进行操作
  //计算均值方差
  double sum_value = 0.0;
  for (int i=0; i<rank_len; i++)
  {
    sum_value += rank_data[i];
    }
   double mean_value = sum_value / rank_len;
   // 计算标准差
   sum_value = 0.0;
   for(int i = 0; i<rank_len; i++)
   {
        sum_value += (rank_data[i] - mean_value)*(rank_data[i] - mean_value);
   }
  double  std_value = sqrt(sum_value / (rank_len - 1));

    // 计算上下门限
    double fac = 0.9;
    double value_up = mean_value + fac * std_value;
    double value_down = mean_value - fac * std_value;
    q_data_length = rank_len;
    delete_index_length = 0;
    for(int i =0; i<q_data_length; i++)
    {
      if (rank_data[i] <= value_up && rank_data[i] >= value_down)
      {
        delete_index[delete_index_length] = i;
        delete_index_length++;
        q_data[i] = -1;
        }
       else if(rank_data[i] > value_up)
        {
          q_data[i] = 1;
          }
          else
          {
            q_data[i] = 0;
            }
      }
    
  }

// 进行合并,结果保存在delete_index中
void merge()
{
    int tmp[256] = {0};
    // 本地的delete_index的下标
    int i =0;
    //接收到的数据的下标
    int j =0;
    int tmp_len = 0;
    while(i < delete_index_length && j < receive_index_len )
    {
        if (delete_index[i] < uart_rece_buf[j] )
        {
            tmp[tmp_len] = delete_index[i];
            tmp_len++;
            i++;
        }
        else if (delete_index[i] > uart_rece_buf[j] )
        {
            tmp[tmp_len] = uart_rece_buf[j];
            j++;
            tmp_len++;
        }
        else
        {
            tmp[tmp_len] = delete_index[i];
            i++;
            j++;
            tmp_len++;
        }
    }
    while ( i < delete_index_length)
    {
        tmp[tmp_len] = delete_index[i];
        i++;
        tmp_len++;
    }
    while( j < receive_index_len)
    {
        tmp[tmp_len] = uart_rece_buf[j];
        j++;
        tmp_len++;
    }

    // 将结果保存到delete_index中
    delete_index_length = tmp_len;
    for(int cc = 0; cc <delete_index_length; cc++ )
    {
        delete_index[cc] = tmp[cc];
    }
}

// 进行密钥生成
// 将所有delete_index中的数据保存在inter中
// 将不在delete_index中的数据保存在key中
 void key_generate()
 {
    merge();
     int tmp_delete_index = 0;
     // 缓存inter的数据
     int tmp_choose_data[255] = {0};
     int tmp_choose_len = 0;
   for(int i =0; i < interleave_data_len; i++)
   {
       if ( i == delete_index[tmp_delete_index])
       {
           tmp_choose_data[tmp_choose_len] = interleave_data[i];
           tmp_choose_len++;
           tmp_delete_index++;
       }
       else
       {
           key[key_len] = q_data[i];
           key_len++;
       }
   }
   // 将新的数据全部复制到inter中
   interleave_data_len = tmp_choose_len;
   for(int i =0; i < interleave_data_len; i++)
   {
       interleave_data[i] = tmp_choose_data[i];
   }
}
// 对生成的密钥进行补全
void add_data()
{
    if (key_len % 4 != 0)
    {
        int add_len = 4 - (key_len%4);
        for (int i =0; i< add_len; i++)
        {
            key[key_len] =0;
            key_len++;
        }
    }
}
// 进行纠错编码
void encode()
{
    add_data();
    //进行74汉明码，需要进行编码的次数
    int loop_num = key_len / 4;
    for (int i=0; i<loop_num; i++)
    {
        int a6 = key[i];
        int a5 = key[loop_num+i];
        int a4 = key[2*loop_num+i];
        int a3 = key[3*loop_num+i];
        //int a2 = a6^a4^a3;
        int a2 = (a6+a4+a3)%2;
        Serial.print(a2);
        Serial.print(" ");
        encode_data[encode_len] = a2;
        encode_len++;
        //int a1 = a6^a5^a4;
        int a1 = (a6+a5+a4)%2;
        Serial.print(a1);
        Serial.print(" ");
        encode_data[encode_len] = a1;
        encode_len++;
        //int a0 = a5^a4^a3;
        int a0 = (a5+a4+a3)%2;
        Serial.print(a0);
        Serial.print(" ");
        Serial.print("\n");
        encode_data[encode_len] = a0;
        encode_len++;
    }
}

// 进行纠错解码
void decode_key()
{
    // 首先接受发送过来的纠错编码
     receive_index_len = receive();
     if(receive_index_len != encode_len)
     {
         Serial.print("纠错编码长度不一致\n");
     }
     Serial.print("产生的纠错编码\n");
    print_data(encode_data,encode_len);
     Serial.println("接受到的纠错编码：");
     print_data(uart_rece_buf,receive_index_len);
     int loop_num = int(encode_len/3);
     Serial.println("循环的次数");
     Serial.println(loop_num);
     int choose_index[255]  = {0};
     int choose_len = key_len;
     Serial.println("密钥长度:");
     Serial.println(key_len);
     Serial.println("choose_index的长度");
     Serial.println(choose_len);
     for (int i=0; i< loop_num; i++)
     {
//        Serial.print("当前的i: ");
//        Serial.println(i);
         bool flag = false;
//         int j =0;
//         int xtpsb = 3;
//         while( j < xtpsb)
//         {
//            Serial.print("当前的j: ");
//            Serial.println(j);
//            Serial.println(flag);  
//            if (uart_rece_buf[j+i*3] != encode_data[j+i*3])
//            {
//                flag = false;
//                break;
//            }
//            j++;
//         }
        if(uart_rece_buf[i*3] == encode_data[i*3] && uart_rece_buf[1+i*3] == encode_data[1+i*3] && uart_rece_buf[2+i*3] == encode_data[2+i*3])
        {
          flag = true;
          }
         //Serial.println("当前的flag");
         //Serial.println(flag);
         //Serial.print("比较值：");
         //Serial.println(flag == false);
         if (flag == false)
         {
             Serial.println(i);
             choose_index[i] = -1;
             choose_index[i+loop_num] = -1;
             choose_index[i+2*loop_num] = -1;
             choose_index[i+3*loop_num] = -1;
         }
     }
     Serial.println("比较的结果:");
     print_data(choose_index, choose_len);
     final_len = 0;
     for(int i=0; i< key_len; i++)
     {
         if (choose_index[i] != -1)
         {
             final_key[final_len] = key[i];
             final_len++;
         }
     }
}
// 得到最后的密钥
void get_byte_key()
{
  //没有byte化final_key, final_len
  //存储的目标 byte_key byte_key_len
  byte_key_len = final_len/8;
  for (int i=0; i<byte_key_len; i++)
  {
    int tmp =0;
    for(int j=0; j < 8; j++)
    {
      tmp += final_key[i*8+j]*pow(2,j);
      }
      key[i] = tmp;
    }
  }

// 进行最后的数据传输
void translate()
{
  for(int i=0; i< 2; i++)
  {
    send_data(key, byte_key_len);
    if(get_ack())
    {
      Serial.println("发送成功");
      }
      else
      {
        Serial.println("发送失败");
        }
    }
  }
void setup() {
   Serial1.begin(38400); //set up serial library baud rate to 115200
   Serial.begin(115200);
   pinMode(LED_BUILTIN, OUTPUT);
   digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
   get_rssi();
   // 首先进行交织
   interleave();
   // 数据长度还是大于10的时候，持续进行生成
   while(interleave_data_len > 10)
   {
    Serial.print("当前的密钥长度:");
    Serial.print(key_len);
    Serial.print("\n");
    Serial.print("剩余数据长度:");
    Serial.print(interleave_data_len);
    Serial.print("\n");
    // 首先进行平滑
    //Serial.print("开始进行smooth\n");
    smooth();
    //Serial.print("smooth的长度为:");
    //Serial.print(smooth_len);
    //Serial.print("\n");
    // 进行rank
    //Serial.print("开始进行rank\n");
    rank();
    //Serial.print("rank的长度为:");
    //Serial.print(rank_len);
    //Serial.print("\n");
    // 进行双门限量化
    //Serial.print("开始进行量化\n");
    double_q();
    //Serial.print("double_q的长度为:");
    //Serial.print(q_data_length);
    Serial.print("\n");
    Serial.print("deleta_index的长度为:");
    Serial.print(delete_index_length);
    Serial.print("\n");
    if (delete_index_length==0)
    {
      int tmp[1] = {255};
      send_data(tmp, 1);
      }
      else
      {
        send_data(delete_index,delete_index_length);
       }
     // 接受得到的index
     Serial.print("此次发送的数据\n");
     print_data(delete_index,delete_index_length);
     Serial.print("发送完数据\n");
     receive_index_len = receive();
     Serial.print("此次接受到的数据:\n");
     print_data(uart_rece_buf ,receive_index_len);
     // 下面就是进行更新
     // 一个数inter_data需要进行更新
     // 一个是key要进行增加
     key_generate();
    }
    Serial.print("未纠错的密钥长度\n");
    Serial.print(key_len);
    Serial.print("\n");
    print_data(key, key_len);
    Serial.print("\n");
    // 下面进行纠错编码
    Serial.print("开始纠错编码\n");
    encode();
    Serial.print("产生的纠错编码\n");
    print_data(encode_data,encode_len);
    Serial.print("\n"); 
    send_data(encode_data,encode_len);
    
    Serial.print("发送完纠错编码\n");
    decode_key();
    //进行解码
    Serial.print("最后的密钥长度:");
    Serial.print(final_len);
    Serial.print("\n");
    print_data(final_key, final_len);
    get_byte_key();
    Serial.println("最后的密钥:");
    print_data(key, byte_key_len);
    send_end();
    translate();
   digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
}

void loop() {
}
