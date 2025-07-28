# mini-pgw

Запуск:
- Вручную (или скриптом) запускать нужно из директорий где лежат бинарники, иначе не найдут конфигурации. Логи и журналы пишутся туда-же.
- Проверить совместную работу можно, запустив из главной директории /test/load_test.sh. Только он возможно сам не завершиться, так как скорее всего получит пакетов меньше чем отправил, а самостоятельно завершается клиент только по получении такого же числа ответов, сколько IMSI он успешно отправил.

HTTP API, примеры:
- curl http://`http_server_ip:port`/stop - вызывает gracefull_offload
- curl http://`http_server_ip:port`/check_subscriber -H "IMSI: `IMSI`"

## Как это работает  
Есть 3 части: 
- IO поток, выполняющий функцию `IO_Worker::run`. Осуществляет все сетевое взаимодействие, общается с потоком обработки через SPSC очереди фиксированного размера (в плане числа пакетов, а не их размера), по которым передает пакеты из сети и получает ответы на отправку (их 4 штуки, по две на каждый протокол: UDP, TCP (HTTP)).
- Поток обработки, забирающий пакеты из очередей поочереди и отправляющий их в соответствующий обработчик.
- Может не совсем отдельная часть, но: Хранилище для сессий. Попытался сделать, чтобы его было удобнее масштабировать, потому оно поделено на шарды и, как следствие, к нему должно быть удобно осуществлять доступ, если нужно найти IMSI который находится в шарде, в который сейчас ничего не пишут. Также паралельно там работает поток очистки, который раз в некоторое время проверяет все сессии в хранилище на истечение срока существования (этот поток тоже причина для существования шардов, ведь получается, что в хранилище постоянно что-то удаляют). И надеюсь, я правильно понял смысл `gracefull_offload_rate`, так как в соответствии с ним я удаляю указанное число сессий в секунду при выгрузке хранилища. Логика функций `_create`, `_update` несколько нарушена.

Пытался соответствовать принципу открытости-закрытости, так что в коде есть лишние на данный момент вещи, например, класс `TCP_Socket` и все с ним связанное (`TCP_Connection`, `TCP_Packet`, `TCP_Handler`), это было для того, чтобы можно было меньшим количеством действий добавить новые типы пакетов, соединений и обработчиков.

Функции IO вынес в отдельную библиотеку, попытался ее абстрагировать от функций самого PGW, чтобы она была просто средством для передачи пакетов куда скажут и получения откуда пришло, хотя, например, можно было добавить на урровень IO отброс несодержательных UDP запросов, чтобы разгрузить обработчики, но тогда потерялась бы некоторая абстракция.  

### Нюансы

- Если что, я спрашивал у Виктора Васильева по поводу изменения UDP-ответов, так что это вроде как согласовано.
- На клиенте генерируется больше одного запроса с IMSI, если добавить параметр -N <Число_IMSI>, это тоже согласовано с Виктором.
- Как написано в спецификации, IMSI кодируется как TBCD (эта кодировка содержит в себе не только цифры и филлер, но и *, #, a, b, c), но я буду принимать их как BCD, то есть только цифры и филлер.
- Уровень логирования NOTICE не использовался, так как его нет в ТЗ
- В клиенте те выводы что нужно по заданию идут на уровнях INFO и выше. На уровне debug просто справочная информация, не соответствующая ТЗ.
- На сервере возможна горячая смена конфигурации, а конкретно таймаута сессии, скорости gracefull offload и уровня логирования. Просто редактируете файл во время работы, основной поток это замечает и меняет конфигурацию.
- А еще я забыл убрать из UDP_Handler более не нужный blacklist
# Попытка в UML
```mermaid
classDiagram
%% ================= PGW Namespace ================= %%
class IMSI {
    - imsi: string
    + set_IMSI_from_str(string): bool
    + set_IMSI_from_IE(vector~uint8_t~): bool
    + get_IMSI_to_str() string
    + get_IMSI_to_IE() vector~uint8_t~
}

class Session {
    + imsi: IMSI
    + last_activity: chrono::steady_clock::time_point
}

class ISession_Storage {
    <<interface>>
    + _create(IMSI, Session) bool
    + _read(IMSI, Session&) bool
    + _update(IMSI, Session) bool
    + _delete(IMSI) bool
}

class Session_Storage {
    - session_timeout_in_seconds: atomic~size_t~&
    - graceful_shutdown_rate: atomic~size_t~&
    - cdr_log: CDR_Journal&
    - blacklist: unordered_set~IMSI~
    - logger: Logger*
    - cleanup(atomic~bool~&) void
    - delete_sessions_gracefully() void
}

class CDR_Journal {
    - logger: Logger*
    - filename: string
    - cdr_max_length_lines: size_t
    - create_file() bool
    + write(IMSI, string) void
    + is_open() bool
}

class Config {
    - config_path: string
    - json_config: unique_ptr~nlohmann::json~
    - last_write_time: file_clock::time_point
    + udp_ip: string
    + udp_port: uint16_t
    + http_ip: string
    + http_port: uint16_t
    + session_timeout_sec: size_t
    + gracefull_shutdown_rate: size_t
    + cdr_file: string
    + cdr_file_max_lines: size_t
    + log_file: string
    + log_level: LogLevel
    + blacklist: vector~string~
    + try_reload() bool
    - load_unreloadable() void
    - load_reloadable() void
}

class Handler {
    # create_response(string) vector~uint8_t~
    + handle_packet(unique_ptr~Packet~) unique_ptr~Packet~
}

class UDP_Handler {
    - logger: Loggerre
    - session_storage: shared_ptr~ISession_Storage~
    + handle_packet(unique_ptr~Packet~) unique_ptr~Packet~
}

class TCP_Handler {
    + handle_packet(unique_ptr~Packet~) unique_ptr~Packet~
}

class HTTP_Handler {
    - session_storage: shared_ptr~ISession_Storage~
    - stop: atomic~bool~&
    - logger: Logger*
    + handle_packet(unique_ptr~Packet~) unique_ptr~Packet~
    - create_error_response(int, const string&) vector~uint8_t~
    - process_request(...) vector~uint8_t~
}

%% ============== IO_Utils Namespace ============== %%
class IRegistrar {
    <<interface>>
    + register_socket(int, uint32_t) int
    + deregister_socket(int) int
    + get_epoll_fd() int
}

class Registrar {
    - epoll_fd: int
}

class Socket {
    # BUFF_SIZE: size_t
    + ip: uint32_t
    + port: uint16_t
    + socket_to_str() string
    + make_ip_address(string, uint32_t&) static int
    + listen_or_bind() virtual int
}

class UDP_Socket {
    + accept_socket(int) int
    + connect_socket() int
}

class TCP_Socket {
    + accept_socket(int) int
    + connect_socket() int
}

class HTTP_Socket {
    // Inherits TCP_Socket
}

class Packet {
    # socket: shared_ptr~Socket~
    + data: vector~uint8_t~
    + get_socket() shared_ptr~Socket~
    + set_socket(shared_ptr~Socket~) void
}

class UDP_Packet {
    // Inherits Packet
}

class TCP_Packet {
    // Inherits Packet
}

class HTTP_Packet {
    // Inherits TCP_Packet
}

class Connection {
    + fd: int
    + send_packet(const Packet&) virtual int
    + recv_packet(Packet&) virtual int
}

class UDP_Connection {
    + send_packet(const Packet&) int
    + recv_packet(Packet&) int
}

class TCP_Connection {
    + send_packet(const Packet&) int
    + recv_packet(Packet&) int
}

class HTTP_Connection {
    // Inherits TCP_Connection
}

class Queue~T~ {
    - capacity: size_t
    - buffer: unique_ptr~unique_ptr~T~[]~
    + push(unique_ptr~T~) bool
    + pop() unique_ptr~T~
}

class IO_Worker {
    - http_server: shared_ptr~Socket~
    - udp_server: shared_ptr~Socket~
    - http_server_fd: int
    - udp_server_fd: int
    - logger: Logger*
    - registrar: unique_ptr~IRegistrar~
    - udp_server_connection: unique_ptr~UDP_Connection~
    - connections: unordered_map~int, unique_ptr~Connection~~
    - client_sockets: unordered_map~int, shared_ptr~Socket~~
    + run(atomic~bool~&, Queue~Packet~&, ...) void
}

%% ================= Relationships ================= %%
Handler <|-- UDP_Handler
Handler <|-- TCP_Handler
TCP_Handler <|-- HTTP_Handler

ISession_Storage <|.. Session_Storage

Session_Storage "1" *-- "1" CDR_Journal
Session_Storage "1" o-- "1" Config
Session_Storage "1" o-- "0..*" IMSI : blacklist
Session_Storage "1" -- "0..*" Session : manages

UDP_Handler o-- "1" ISession_Storage
HTTP_Handler o-- "1" ISession_Storage

IMSI -- Session : composition

Socket <|-- UDP_Socket
Socket <|-- TCP_Socket
TCP_Socket <|-- HTTP_Socket

Packet <|-- UDP_Packet
Packet <|-- TCP_Packet
TCP_Packet <|-- HTTP_Packet

Connection <|-- UDP_Connection
Connection <|-- TCP_Connection
TCP_Connection <|-- HTTP_Connection

IRegistrar <|.. Registrar

IO_Worker o-- "1" IRegistrar
IO_Worker o-- "1" UDP_Connection
IO_Worker o-- "0..*" Connection
IO_Worker o-- "0..*" Socket : client_sockets
IO_Worker o-- "1" Socket : http_server
IO_Worker o-- "1" Socket : udp_server

IO_Worker ..> Queue~Packet~ : uses
IO_Worker ..> Logger : uses
Session_Storage ..> Logger : uses
CDR_Journal ..> Logger : uses
