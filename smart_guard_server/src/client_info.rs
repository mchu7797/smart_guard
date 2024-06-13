use std::net::SocketAddr;

pub struct ClientInfo {
    pub address: SocketAddr,
    pub warning_count: u64,
}

impl ClientInfo {
    pub fn new(address: SocketAddr) -> Self {
        Self {
            address: address,
            warning_count: 0u64,
        }
    }

    pub fn grow_count(&mut self) {
        self.warning_count += 1
    }
}
