mod client_info;

use chrono::{DateTime, FixedOffset, TimeZone};
use client_info::ClientInfo;
use std::collections::HashMap;
use std::fs::{self, File};
use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use std::path::Path;
use std::process::Command;
use std::sync::{Arc, Mutex};
use std::thread;

fn handle_client(mut stream: TcpStream, clients: Arc<Mutex<HashMap<i64, ClientInfo>>>) {
    let mut buffer = [0; 32];
    let mut client_id: i64 = 0;

    loop {
        match stream.read_exact(&mut buffer) {
            Ok(_) => {
                let command = i32::from_le_bytes(buffer[0..4].try_into().unwrap());
                let image_size = i64::from_le_bytes(buffer[16..24].try_into().unwrap());
                let epoch_time_raw = i64::from_le_bytes(buffer[24..32].try_into().unwrap());
                
                // Save from korean standard timezone
                let epoch_time = FixedOffset::east_opt(9 * 3600).unwrap().from_utc_datetime(
                    &DateTime::from_timestamp(epoch_time_raw, 0)
                        .unwrap()
                        .naive_utc(),
                );

                client_id = i64::from_le_bytes(buffer[8..16].try_into().unwrap());

                match command {
                    0 => {
                        // Init command
                        let mut clients = clients.lock().unwrap();
                        clients.insert(client_id, ClientInfo::new(stream.peer_addr().unwrap()));
                        stream.write_all(&[1]).unwrap();

                        println!(
                            "When {}, Client {}, ID = {}: Connection opened!",
                            epoch_time.format("%F %T"),
                            stream.peer_addr().unwrap(),
                            client_id
                        );
                    }
                    1 => {
                        // Warning message
                        let mut clients = clients.lock().unwrap();
                        if let Some(client_info) = clients.get_mut(&client_id) {
                            stream.write_all(&[1]).unwrap();

                            let mut image_data = vec![0; image_size as usize];
                            stream.read_exact(&mut image_data).unwrap();

                            let file_name = format!(
                                "images/{}_{}_{}.png",
                                client_id,
                                epoch_time.format("%F_%H%M"),
                                client_info.warning_count
                            );
                            let mut file = File::create(&file_name).unwrap();
                            file.write_all(&image_data).unwrap();

                            stream.write_all(&[1]).unwrap();

                            client_info.grow_count();

                            // For open image file on windows
                            let win_path = file_name.replace("/", "\\");

                            // Show image viewer
                            Command::new("explorer.exe")
                                .arg(&win_path)
                                .spawn()
                                .expect("Failed to open image viewer");

                            // Console alert
                            println!(
                                "When {}, Client {}, ID = {}: Suspect detected!",
                                epoch_time.format("%F %T"),
                                client_info.address.to_string(),
                                client_id
                            );
                        } else {
                            stream.write_all(&[0]).unwrap();
                            println!(
                                "When {} Client {}, ID = {}: Cannot found communication stream!",
                                epoch_time.format("%F %T"),
                                stream.peer_addr().unwrap().to_string(),
                                client_id
                            );
                        }
                    }
                    2 => {
                        // Exit command
                        let mut clients = clients.lock().unwrap();
                        let client = clients.remove(&client_id);
                        stream.write_all(&[1]).unwrap();

                        println!(
                            "When {}, Client {}, ID = {}: Connection closed!",
                            epoch_time.format("%F %T"),
                            client.unwrap().address.to_string(),
                            client_id
                        );

                        break; // Exit the loop
                    }
                    _ => {
                        stream.write_all(&[0]).unwrap();
                    }
                }
            }
            Err(_) => {
                println!(
                    "Client {}: Connection closed by unexpected reason!",
                    client_id
                );
                let mut clients = clients.lock().unwrap();
                clients.remove(&client_id);
                break; // Exit the loop
            }
        }
    }
}

fn main() {
    let listener = TcpListener::bind("0.0.0.0:12877").unwrap();
    let clients: Arc<Mutex<HashMap<i64, ClientInfo>>> = Arc::new(Mutex::new(HashMap::new()));
    let image_folder_path = Path::new("images");

    if !image_folder_path.exists() {
        fs::create_dir(image_folder_path).expect("Failed with create image folder!");
    }

    for stream in listener.incoming() {
        let stream = stream.unwrap();
        let clients = clients.clone();
        thread::spawn(|| handle_client(stream, clients));
    }
}
