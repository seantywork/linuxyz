mod world;

use std::{
    collections::HashMap,
    fs,
    env,
    mem,
    rc::Rc,
    io::{self, Read, Write, Error, BufReader},
    net::{TcpListener, TcpStream},
    thread,
    sync::mpsc,
    sync::{Arc, Mutex},
    time::Duration,
};




// basic var

fn multiply_by_two(x: i64) -> i64{

    let mut result = 0;

    result = x * 2;

    return result;

}

fn minus_one(x: &mut i64) {

    *x -= 1;

}


fn basic_var(){


    let mut tryme = 0;

    for i in 0..11 {

        if i % 2 == 0 {

            tryme += i;

        } else {

            continue;

        }
    }

    println!("before: {}", tryme);

    tryme = multiply_by_two(tryme);

    println!("multplied: {}", tryme);

    minus_one(&mut tryme);

    println!("tryme: {}", tryme);

}


// basic slice

fn sum_el(arr: &mut [i64]) -> i64 {

    let mut ans = 0;

    let arrlen = arr.len();

    for i in 0..arrlen{

        ans += arr[i];
    }


    return ans;
}


fn change_el_at(arr: &mut [i64], i: usize) -> i32{


    let arrlen = arr.len() as i32;


    if arrlen == 0 {

        return -1;
    }

    if i as i32 >= arrlen {

        return -2;

    }

    arr[i] = arr[i] - 5;

    return 0;

}

fn sum_vec_el(vec: &mut Vec<i64>) -> i64{


    let mut ans: i64 = 0;

    let veclen = vec.len();

    for i in 0..veclen{

        ans += vec[i];
    }


    return ans;

}


fn change_vec_el_at(vec: &mut Vec<i64>, i: usize) -> i32 {



    let veclen = vec.len() as i32;


    if veclen == 0 {

        return -1;
    }

    if i as i32 >= veclen {

        return -2;

    }

    vec[i] = vec[i] - 500;

    return 0;
}


fn basic_slice(){

    let arr: &mut [i64] = &mut [5, 10, 15, 20];


    let vec = &mut Vec::<i64>::new();

    vec.push(100);
    vec.push(200);
    vec.push(300);



    let mut sum = sum_el(arr);

    println!("sum el: {}", sum);

    let mut res = change_el_at(arr, 6);

    println!("change res: {}", res);

    res = change_el_at(arr, 2);

    println!("change res: {}", res);

    sum = sum_el(arr);

    println!("changed sum: {}", sum);

    sum = sum_vec_el(vec);

    println!("vec sum el: {}", sum);

    res = change_vec_el_at(vec, 6);

    println!("change vec res: {}", res);

    res = change_vec_el_at(vec, 2);

    println!("change vec res: {}", res);

    sum = sum_vec_el(vec);

    println!("changed vec sum el: {}", sum);


}


// basic string



fn append_two_string(a: &mut String, b: &mut String) -> String{

    let mut retstr = String::new();

    retstr += a;

    retstr += b;

    return retstr;

}


fn basic_string(){


    let a = &mut "hello ".to_string();

    let b = &mut "you".to_string();

    let c = append_two_string(a, b);

    println!("basic string: {}", c);


}


// basic struct


struct Whatever {

    name: String,
    numbers: Vec<i64>

}

impl Whatever {

    fn make(name: &String, numbers: &mut [i64]) -> Whatever {

        let mut w = Whatever{
            name: "".to_string(),
            numbers: Vec::<i64>::new()
        };

        let numlen = numbers.len();

        w.name = name.to_string();

        for i in 0..numlen{

            w.numbers.push(numbers[i]);

        }

        return w

    }

}

fn edit_whatever(x: &mut Whatever){

    x.name = "amazing".to_string() + &x.name;

    x.numbers.push(1);

    x.numbers.push(2);

    x.numbers.push(3);


}

fn show_whatever(x: &mut Whatever){


    println!("name: {}", x.name);

    println!("numbers: {:?}", x.numbers);

}


fn basic_struct(){


    let mut w = Whatever::make(&"idiot".to_string(), &mut [10,20,30,40]);

    edit_whatever(&mut w);

    show_whatever(&mut w);

}


fn show_map(themap: &mut HashMap<String, i32>){

    for (k, v) in themap {

        println!("{k}: {v}");
    }

}

fn find_in_map(themap: &mut HashMap<String, i32>, key: &String){

    let target = themap.get(key).copied().unwrap_or(-1);

    if (target == -1){

        println!("key: {key} not found");

    } else {

        println!("key: {key}, val: {target}");

    }
}


fn edit_map(themap: &mut HashMap<String, i32>, key: &String, val: i32){

    themap.insert(key.to_string(), val);

}



fn basic_map(){

    let mut themap = HashMap::new();

    themap.insert(String::from("Blue"), 10);
    themap.insert(String::from("Yellow"), 50);

    let rightkey = String::from("Blue");

    let wrongkey = String::from("blue");

    show_map(&mut themap);

    find_in_map(&mut themap, &rightkey);

    find_in_map(&mut themap, &wrongkey);

    edit_map(&mut themap, &rightkey, 100);

    show_map(&mut themap);


}


fn get_box(a: &str) -> Box<String>{


    let retbox = Box::new(a.to_string());

    return retbox;
}


fn get_rc(a: &str) -> Rc<String>{


    let retrc = Rc::new(a.to_string());

    return retrc;
}


fn get_arc(a: &str) -> Arc<String>{

    let retarc = Arc::new(a.to_string());

    return retarc;
}


fn print_string(a: String){
    println!("{}", a);
}

fn print_string_rc(a: Rc<String>){

    println!("{}", a);
}

fn print_string_arc(a: Arc<String>){

    println!("{}", a);
}

fn basic_pointer(){

    let word = String::from("hello");

    print_string(word);

    // print_string(word);

    let wordbox = get_box("box");

    let wordboxout = wordbox.to_string();

    print_string(wordboxout);

    // print_string(wordboxout);

    let wordrc = get_rc("rc");

    let wordrc2 = wordrc.clone();

    print_string_rc(wordrc);

    print_string_rc(wordrc2);

    let wordarc = get_arc("arc");

    let wordarc2 = wordarc.clone();

    print_string_arc(wordarc);

    print_string_arc(wordarc2);



}


fn basic_lock(){

    let locker = Arc::new(Mutex::new(0));
    let mut handles = vec![];


    for _ in 0..10 {

        let locker = Arc::clone(&locker);
        
        let handle = thread::spawn(move || {
            
            let mut locked = locker.lock().unwrap();

            let mut countnow = *locked;

            *locked = countnow + 1;
            
        });
        handles.push(handle);
    }

    for handle in handles {
        handle.join().unwrap();
    }

    println!("Result: {}", locker.lock().unwrap());

}



fn basic_channel(){

    let (tx, rx) = mpsc::channel();

    thread::spawn(move || {
        let vals = vec![
            String::from("hi"),
            String::from("from"),
            String::from("the"),
            String::from("thread"),
        ];

        for val in vals {
            tx.send(val).unwrap();
            thread::sleep(Duration::from_secs(1));
        }
    });

    for received in rx {
        println!("Got: {received}");
    }

}


fn basic_file(){
    

    let file_path = "test.txt".to_string();

    let contents = fs::read_to_string(file_path).expect("no file");

    println!("contents: \n{contents}");


}


fn handle_connection(mut stream: TcpStream) {

    println!("new client {}", stream.peer_addr().unwrap());

    let mut buf = [0; 4096];

    loop {

        let rval = stream.read(&mut buf);

        match rval {
            Ok(n) => {

                let mut response = "response okay: ".to_string();
                
                let mut data = String::from_utf8_lossy(&buf[0..n]).to_string();

                response = response + data.as_mut_str();

                stream.write_all(response.as_bytes()).unwrap();
            }
            Err(e)=>{

                println!("read error: {}", e)
            }
        }

    }

}

fn run_server(){

    let listener = TcpListener::bind("0.0.0.0:8080").unwrap();

    for stream in listener.incoming() {

        match stream {
            Ok(stream) =>{
                thread::spawn(|| {
                    handle_connection(stream);
                });
            }
            Err(e) => {
                println!("accept error: {}", e);
            }

        }

    }
}

fn basic_tcp(){

    thread::spawn(||{
        run_server();
    });

    type Port = u16;

    let host = "localhost".to_string();

    let port = "8080".to_string().parse::<Port>().unwrap();

    let mut stream = TcpStream::connect((host.as_str(), port)).unwrap();

    let mut input_stream = stream.try_clone().unwrap();

    let handler = thread::spawn(move || {
        let mut client_buffer = [0u8; 1024];

        loop {
            match input_stream.read(&mut client_buffer) {
                Ok(n) => {
                    if n == 0 {
                        println!("n == 0");
                        return;
                    }
                    else
                    {
                        io::stdout().write(&client_buffer).unwrap();
                        io::stdout().flush().unwrap();
                    }
                },
                Err(error) => println!("{}", error.to_string()),
            }
        }
    });

    let output_stream = &mut stream;
    let mut user_buffer = String::new();

    loop {
        io::stdin().read_line(&mut user_buffer).unwrap();

        output_stream.write(user_buffer.as_bytes()).unwrap();
        output_stream.flush().unwrap();
    }

}


fn basic_module(){

    let retwords= world::speak(&"what the hell do you want?");

    println!("{}",retwords);

    let mut spoken = world::Spoken{
        raw: &"",
    };

    let mut inner = "damn";

    let mut result = spoken.insert(inner);

    match  result {
        Ok(done) => {

            println!("insert success");

        },
        Err(err) => {

            println!("insert errer: {}", err);
        }

    }

    let mut out = spoken.heart_out();

    let mut outstr = out.to_string();

    println!("spoken: {}", outstr);

    inner = "super idiots";

    spoken.insert(inner);

    out = spoken.heart_out();

    outstr = out.to_string();

    println!("spoken: {}", outstr);


}


fn main(){

    // basic_var();

    // basic_slice();

    // basic_string();

    // basic_struct();

    // basic_map();

    // basic_pointer();

    // basic_lock();

    // basic_channel();

    // basic_file();

    // basic_tcp();

    basic_module();
}