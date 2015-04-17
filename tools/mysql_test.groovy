import groovy.sql.Sql
import java.util.concurrent.Callable
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicLong
import java.util.regex.Matcher
import java.util.regex.Pattern

class mysql_test {
  //数据库连接url
  String url
  //用户名
  String usr
  //密码
  String pwd
  //需要操作的表名
  String table_name
  //分表数目
  int table_num = 0
  //并发线程数
  int thread_num = 1
  //运行次数
  long run_times = 1
  //统计间隔时间
  int interval = 1
  //执行总数
  private long total = 0
  //每秒执行数
  private long second_num = 0
  //已停止的线程
  private AtomicInteger stop_thread_number = new AtomicInteger(0)
  //生成value列表的方法
  private Map function_map = new HashMap()
  //sql对应的ratio
  private Map<String, Integer> ratio_map = new HashMap<String, Integer>()
  //执行队列
  private ArrayDeque queue = new ArrayDeque()
  //原始队列
  private Collection o_queue = new ArrayList()
  //自增id
  AtomicLong id_seq = new AtomicLong(1L)
  //监控数据
  private Map moniter_data = new HashMap()
  private Map moniter_total = new HashMap()
  private Map moniter_time = new HashMap()

  private Pattern pattern = Pattern.compile("#(.*?)#");

  def init_db(String init_sql) {
    Sql client = connent();
    try {
      execute(client, init_sql, table_name)
    } finally {
      client.close()
    }
  }

  def truncate_db() {
    Sql client = connent();
    try {
      execute(client, "truncate table #table_name#", table_name)
    } finally {
      client.close()
    }
  }

  def add_sql(String sql, Closure closure, int ratio) {
    function_map.put(sql, closure)
    ratio_map.put(sql, ratio)
    moniter_data.put(sql, 0)
    moniter_total.put(sql, 0)
    moniter_time.put(sql, 0)
  }

  def run() {
    init_o_queue()
    def pool = Executors.newFixedThreadPool(thread_num + 1)
    def defer = { c -> pool.submit(c as Callable) }
    1.upto(thread_num) { defer {operate_data()} }
    defer {moniter()}
    pool.shutdown()
  }

  private moniter() {
    println "moniter start!"
    long start = System.currentTimeMillis();
    while (stop_thread_number.get() < thread_num) {
      def t = System.currentTimeMillis();
      Thread.sleep(interval * 1000)
      def inter = (System.currentTimeMillis() - t) / 1000
      println "per/s:" + second_num / inter + " ,total:" + total
      second_num = 0
      moniter_data.each {m ->
        println "sql: " + m.key + "\n per/s : " + m.value / inter
        m.value = 0
      }
      println "----------------------------------------------------------------------"
    }
    BigDecimal decimal = (System.currentTimeMillis() - start) / 1000
    println "total use:" + decimal + "s,avg per/s:" + total / decimal + "\n"
    moniter_total.each {m ->
      println "sql:" + m.key + "\n total:" + m.value
      def total_time = moniter_time.get(m.key)
      println "all use time:" + (total_time / 1000) + "s"
      println "avg per/s:" + m.value / decimal
      println "avg rt:" + (total_time / m.value / 1000) + "s"
      println "________"
    }

  }

  private init_o_queue() {
    ratio_map.each { r -> 1.upto(r.value) {o_queue.add(r.key) } }
  }

  private synchronized get_sql() {
    if (queue.size() == 0) {
      queue.addAll(o_queue)
    }
    return queue.poll()
  }

  private Sql connent() {
    return Sql.newInstance(url, usr, pwd, "com.mysql.jdbc.Driver")
  }

  private operate_data() {
    Sql client = connent();
    try {
      while (total < run_times) {
        try {
          String sql = get_sql()
          def function = function_map.get(sql)
          def values = function.call(id_seq.getAndIncrement())
          if (values instanceof Map) {
            String t_n = table_num <= 1 ?
              table_name :
              table_name + "_" + (values["id"] % table_num).toString().padLeft(table_num.toString().length(), "0")
            values.put("table_name", t_n)
            String sql_final = formatStr(sql, values)
            long start = System.currentTimeMillis()
            client.execute(sql_final)
            incr(sql, (System.currentTimeMillis() - start))
          } else if (values instanceof List) {
            long start = System.currentTimeMillis()
            client.execute(sql, values)
            incr(sql, (System.currentTimeMillis() - start))
          }
        } catch (Exception e) {
          e.printStackTrace()
        }
      }
    } finally {
      client.close()
      stop_thread_number.incrementAndGet()
    }
  }


  private incr(String sql, long time) {
    total++
    second_num++
    moniter_data.put(sql, moniter_data.get(sql) + 1)
    moniter_time.put(sql, moniter_time.get(sql) + time)
    moniter_total.put(sql, moniter_total.get(sql) + 1)
  }

  private execute(Sql client, String sql, String table_name, Map values = new HashMap(), boolean one_table = false) {
    if (table_num <= 1 || one_table) {
      values.put("table_name", table_name)
      client.execute(formatStr(sql, values))
    } else {
      0.upto(table_num - 1) {
        String t_n = table_name + "_" + it.toString().padLeft(table_num.toString().length(), "0")
        values.put("table_name", t_n)
        client.execute(formatStr(sql, values))
        println "execute done!the done table is:" + t_n + " ,process is:" + (it + 1) + " in " + table_num
      }
    }
  }

  private String formatStr(String format, Map pars) {
    Matcher m = pattern.matcher(format);
    while (m.find()) {
      def parname = m.group();
      def parname_inside = m.group(1);
      def par = pars.get(parname_inside);
      if (par != null) {
        format = format.replaceAll(parname, par.toString());
      }
    }
    return format;
  }
}


