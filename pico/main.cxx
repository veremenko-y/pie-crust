#define PARAM_ASSERTIONS_DISABLE_ALL 1

//#define CYW43_CHIPSET_FIRMWARE_INCLUDE_FILE "flash_firmware.h"
#include "pico/stdlib.h"
#include "pico/platform.h"
//#define wifi_nvram_4343 __in_flash("wifi_firmware") wifi_nvram_4343
//#define CYW43_CHIPSET_FIRMWARE_INCLUDE_FILE __attribute__((aligned(4))) __in_flash("wifi_firmware")

#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "lwip/dns.h"

#include "bus.pio.h"

#define DEBUG_LEVEL 2

#if DEBUG_LEVEL >= 1
#define DEBUG1(...)          \
    do                       \
    {                        \
        printf(__VA_ARGS__); \
    } while (false)
#else
#define DEBUG1(...) \
    do              \
    {               \
    } while (false)
#endif

#if DEBUG_LEVEL >= 2
#define DEBUG2(...)          \
    do                       \
    {                        \
        printf(__VA_ARGS__); \
    } while (false)
#else
#define DEBUG2(...) \
    do              \
    {               \
    } while (false)
#endif

#if DEBUG_LEVEL >= 3
#define DEBUG3(...)          \
    do                       \
    {                        \
        printf(__VA_ARGS__); \
    } while (false)
#else
#define DEBUG3(...) \
    do              \
    {               \
    } while (false)
#endif

#define inhibit_pin 6
#define reset_pin 3

#define BUS_READ (1 << 3)
#define BUS_IOSELECT (1 << 2)

#define STATUS_BUSY (1 << 7)
#define STATUS_DATAREADY (1 << 6)
#define STATUS_WIFICONNECTED (1 << 5)
#define STATUS_ERROR (1 << 4)

#define IO_CMD 2
#define IO_DATA 3

#define CMD_WRITEDATA 0
#define CMD_WIFI 1
#define CMD_READ_VEDRIVE_BLOCK 2
#define CMD_WRITE_VEDRIVE_BLOCK 3
#define CMD_GET_FIRMWARE 0xFE
#define CMD_DEBUG 0xFF

extern const __attribute__((aligned(4))) uint8_t drv0boot[1024];
extern const __attribute__((aligned(4))) uint8_t drv1drive[1024];

const uint8_t* const firmware_files[] = {
    drv0boot,
    drv1drive
};

static const constexpr int DataBufferSize = 2048;
__scratch_y("bus") PIO pio;
__scratch_y("bus") uint32_t data_bus_sm;
__scratch_y("bus") uint32_t addr_bus_sm;
__scratch_y("bus") uint32_t pindirs_bus_sm;
__scratch_y("bus") uint32_t io_addr;
__scratch_y("bus") bool io_read;
__scratch_y("bus") volatile uint32_t status = 0;
__scratch_y("bus") uint8_t firmware[512];
__scratch_y("bus") uint32_t io_in[16];
__scratch_y("bus") uint32_t io_out[16];
__scratch_y("bus") uint8_t data_buffer[DataBufferSize];
__scratch_y("bus") uint32_t data_index = 0;
__scratch_y("bus") uint32_t data_count = 0;
__scratch_y("bus") uint32_t last_block_dr = -1;
__scratch_y("bus") uint32_t last_block_hi = -1;
__scratch_y("bus") uint32_t last_block_lo = -1;

void set_buffer(void *data, int count)
{
    memcpy(data_buffer, data, count);
    data_index = 0;
    data_count = count;
    io_out[IO_DATA] = data_buffer[0];
    __dmb();
    status |= STATUS_DATAREADY;
}

void set_buffer(int count)
{
    data_index = 0;
    data_count = count;
    io_out[IO_DATA] = data_buffer[0];
    __dmb();
    status |= STATUS_DATAREADY;
}

void __scratch_y("bus_code") enter_bus_loop()
{
    // DEBUG1("core2: init\n");
    while (true)
    {
        uint data = pio_sm_get_blocking(pio, addr_bus_sm);
        uint addr = data >> 5;

        if (data & BUS_IOSELECT)
        {
            if (data & BUS_READ)
            {
                pio_sm_put(pio, data_bus_sm, firmware[addr]);
            }
            else
            {
                // dummy read from the bus to clear PIO
                pio_sm_get_blocking(pio, data_bus_sm);
            }
        }
        else if (!(status & STATUS_BUSY))
        {
            io_addr = addr & 0x0f;
            if (data & BUS_READ)
            {
                if (io_addr == IO_CMD)
                {
                    pio_sm_put(pio, data_bus_sm, status);
                }
                else
                {
                    pio_sm_put(pio, data_bus_sm, io_out[io_addr]);
                    io_read = true;
                    __dmb();
                    status |= STATUS_BUSY;
                }
            }
            else
            {
                uint raw = pio_sm_get_blocking(pio, data_bus_sm);
                io_read = false;
                io_in[io_addr] = (char)(raw >> 24);
                __dmb();
                status |= STATUS_BUSY;
            }
        }
        else
        {
            // return status or ignore write when busy
            if (data & BUS_READ)
            {
                pio_sm_put(pio, data_bus_sm, status);
            }
            else
            {
                // dummy read from the bus to clear PIO
                pio_sm_get_blocking(pio, data_bus_sm);
            }
        }
    }
}

template <typename T>
struct Result
{
    T value;
    bool success;
    err_t err;

    Result()
    {
        success = false;
        err = ERR_OK;
    }

    bool is_success()
    {
        if (err == ERR_INPROGRESS)
        {
            while (!success)
            {
            }
            err = ERR_OK;
        }
        return err == ERR_OK;
    }
};

void reset_irq()
{
    if (gpio_get_irq_event_mask(reset_pin) & GPIO_IRQ_EDGE_FALL)
    {
        DEBUG1("/RESET\n");
        stdio_flush();
        stdio_set_driver_enabled(&stdio_uart, false);
        for (uint gpio = 0; gpio < 28; gpio++)
            gpio_disable_pulls(gpio);
        gpio_set_dir_all_bits(0);
        multicore_reset_core1();
        pio_set_sm_mask_enabled(pio, 0xf, false);
        busy_wait_until(at_the_end_of_time);
    }
}

uint8_t crc(uint8_t *ptr, int size)
{
    uint8_t crc = 0;
    for (int i = 0; i < size; i++)
    {
        crc = crc ^ ptr[i];
    }
    return crc;
}

struct VeRequest
{
    struct udp_pcb *ntp_pcb;
    bool success;
};

VeRequest veRequest;
ip4_addr_t addr;

static void ntp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    // uint8_t mode = pbuf_get_at(p, 0) & 0x7;
    // uint8_t stratum = pbuf_get_at(p, 1);
    // Check the result
    // if (ip_addr_cmp(addr, &state->ntp_server_address) && port == NTP_PORT && p->tot_len == NTP_MSG_LEN &&
    //     mode == 0x4 && stratum != 0) {
    DEBUG3("Len %d\n", p->tot_len);
    pbuf_copy_partial(p, data_buffer, 512, 10);
    set_buffer(512);

    // } else {
    //     printf("invalid ntp response\n");
    // }
    pbuf_free(p);
    veRequest.success = true;
}

void cmd_handler()
{
    status &= ~STATUS_ERROR;
    switch (io_in[IO_CMD])
    {
    case CMD_WRITEDATA:
    {
        DEBUG3("resetting data ptr\n");
        data_index = 0;
        data_count = 0;
        break;
    }
    case CMD_WIFI:
    {
        DEBUG1("connecting to wifi\n");
        // cyw43_arch_wifi_connect_async("ThisMachineKillsFascists", "25116A125932", CYW43_AUTH_WPA2_AES_PSK);
        int status;
        // while ((status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_AP)) < CYW43_LINK_UP)
        // {
        //     DEBUG1("Waiting on link up. Status %d\n", status);
        //     cyw43_arch_poll();
        //     sleep_ms(1000);
        // }
        DEBUG1("IP Address    : %s\r\n", ipaddr_ntoa(((const ip_addr_t *)&cyw43_state.netif[0].ip_addr)));
        status |= STATUS_WIFICONNECTED;
        break;
    }
    case CMD_READ_VEDRIVE_BLOCK:
    {
        int attempts = 0;
        while (true)
        {
            DEBUG3("req %d\n", attempts++);
            uint8_t drive = data_buffer[0] ? 0x05 : 0x03;
            uint8_t block_lo = data_buffer[1];
            uint8_t block_hi = data_buffer[2];
            if (drive == last_block_dr && block_hi == last_block_hi && block_lo == last_block_lo)
            {
                // cache hit
                set_buffer(512);
                return;
            }

            veRequest.ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
            udp_bind(veRequest.ntp_pcb, IP_ANY_TYPE, 0);

            if (!veRequest.ntp_pcb)
            {
                DEBUG1("failed to create pcb\n");
                return;
            }
            veRequest.success = false;
            udp_recv(veRequest.ntp_pcb, ntp_recv, &veRequest);

            struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, 5, PBUF_RAM);
            uint8_t *req = (uint8_t *)p->payload;
            // memset(req, 0, NTP_MSG_LEN);
            req[0] = 0xc5;
            req[1] = drive;
            req[2] = block_lo;
            req[3] = block_hi;
            req[4] = crc(req, 4);
            IP4_ADDR(&addr, 192, 168, 4, 255);
            cyw43_arch_lwip_begin();
            err_t error = udp_sendto(veRequest.ntp_pcb, p, &addr, 6502);
            cyw43_arch_lwip_end();
            pbuf_free(p);

            DEBUG3("req wait, err = %d\n", error);
            int count = 0;
            while (!veRequest.success)
            {
                cyw43_arch_poll();
                count++;
                if (count > 20000)
                {
                    DEBUG3("req timeout\n");
                    break;
                }
                sleep_us(100);
            }
            udp_remove(veRequest.ntp_pcb);
            DEBUG3("req done\n");
            if (veRequest.success)
            {
                last_block_dr = drive;
                last_block_hi = block_lo;
                last_block_lo = block_hi;
                break;
            }
        }

        break;
    }
    case CMD_WRITE_VEDRIVE_BLOCK:
    {
        int attempts = 0;
        while (true)
        {
            DEBUG3("req %d\n", attempts++);
            uint8_t drive = data_buffer[0] ? 0x04 : 0x02;
            uint8_t block_lo = data_buffer[1];
            uint8_t block_hi = data_buffer[2];
            if (drive == last_block_dr && block_hi == last_block_hi && block_lo == last_block_lo)
            {
                // cache reset
                last_block_dr = -1;
            }

            veRequest.ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
            udp_bind(veRequest.ntp_pcb, IP_ANY_TYPE, 0);

            if (!veRequest.ntp_pcb)
            {
                DEBUG1("failed to create pcb\n");
                return;
            }
            veRequest.success = false;
            udp_recv(veRequest.ntp_pcb, ntp_recv, &veRequest);

            static constexpr int WritePacketSize = 512+6;
            struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, WritePacketSize, PBUF_RAM);
            uint8_t *req = (uint8_t *)p->payload;
            // memset(req, 0, NTP_MSG_LEN);
            req[0] = 0xc5;
            req[1] = drive;
            req[2] = block_lo;
            req[3] = block_hi;
            req[4] = crc(req, 4);
            memcpy(req + 5, data_buffer, 512);
            req[WritePacketSize-1] = crc(req, WritePacketSize-1);
            IP4_ADDR(&addr, 192, 168, 4, 255);
            cyw43_arch_lwip_begin();
            err_t error = udp_sendto(veRequest.ntp_pcb, p, &addr, 6502);
            cyw43_arch_lwip_end();
            pbuf_free(p);

            DEBUG3("req wait, err = %d\n", error);
            int count = 0;
            while (!veRequest.success)
            {
                cyw43_arch_poll();
                count++;
                if (count > 20000)
                {
                    DEBUG3("req timeout\n");
                    break;
                }
                sleep_us(100);
            }
            udp_remove(veRequest.ntp_pcb);
            DEBUG3("req done\n");
            if (veRequest.success)
            {
                break;
            }
        }

        break;
    }
    case CMD_GET_FIRMWARE:
    {
        if(data_buffer[0] < sizeof(firmware_files))
        {
            set_buffer((void *)firmware_files[data_buffer[0]], 1024);
        }
        DEBUG1("get firmware dummy\n");
        break;
    }
    case CMD_DEBUG:
    {
        DEBUG1("status:\n");
        DEBUG1("STATUS_BUSY=%d\n", (STATUS_BUSY & status) > 0);
        DEBUG1("STATUS_DATAREADY=%d\n", (STATUS_DATAREADY & status) > 0);
        DEBUG1("STATUS_WIFICONNECTED=%d\n", (STATUS_WIFICONNECTED & status) > 0);
        DEBUG1("STATUS_ERROR=%d\n", (STATUS_ERROR & status) > 0);
        DEBUG1("data buffer:");
        for (uint i = 0; i < data_count; i++)
        {
            if ((i % 16) == 0)
            {
                printf("\n$%04x:", i);
            }
            printf(" $%02x", data_buffer[i]);
        }
        DEBUG1("\n");
        break;
    }
    default:
        DEBUG1("unk cmd\n");
        status |= STATUS_ERROR;
        break;
    }
}

void data_handler()
{
    DEBUG3("data handler\n");
    if (io_read)
    {
        DEBUG3("data handler read %d %d\n", data_index, data_count);
        if (data_index < data_count)
        {
            io_out[IO_DATA] = data_buffer[++data_index];
        }
        if (data_index >= data_count)
        {
            status &= ~STATUS_DATAREADY;
        }
    }
    else
    {
        DEBUG3("data handler write\n");
        if (data_count < sizeof(data_buffer))
        {
            data_buffer[data_count++] = io_in[IO_DATA];
            DEBUG3("data written:");
            for (uint i = 0; i < data_count; i++)
            {
                if ((i % 16) == 0)
                {
                    DEBUG3("\n$%04x:", i);
                }
                DEBUG3(" $%02x", data_buffer[i]);
            }
            DEBUG3("\n");
        }
    }
}

static void setup_pio()
{
    gpio_set_dir(inhibit_pin, GPIO_OUT);
    gpio_put(inhibit_pin, 0);
    gpio_init(inhibit_pin);
    gpio_set_dir(reset_pin, GPIO_IN);
    gpio_pull_down(reset_pin);
    gpio_add_raw_irq_handler(reset_pin, reset_irq);
    gpio_init(reset_pin);
    gpio_set_dir(enable_pin, GPIO_IN);
    gpio_pull_down(enable_pin);
    gpio_init(enable_pin);
    gpio_set_dir(devselect_pin, GPIO_IN);
    gpio_pull_up(devselect_pin);
    gpio_init(devselect_pin);
    gpio_set_dir(rw_pin, GPIO_IN);
    gpio_pull_down(rw_pin);
    gpio_init(rw_pin);

    uint addr_mask = ~((1 << addr_pins) - 1) & ((1 << (addr_pins + 8)) - 1);
    gpio_set_dir_in_masked(addr_mask);
    for (uint gpio = addr_pins; gpio < addr_pins + 8; gpio++)
        gpio_disable_pulls(gpio);
    gpio_init_mask(addr_mask);

    uint data_mask = ~((1 << data_pins) - 1) & ((1 << (data_pins + 8)) - 1);
    gpio_set_dir_in_masked(data_mask);
    for (uint gpio = data_pins; gpio < data_pins + 8; gpio++)
        gpio_disable_pulls(gpio);
    gpio_init_mask(data_mask);

    memcpy(firmware, drv0boot, sizeof(firmware));

    while (!gpio_get(reset_pin))
    {
        tight_loop_contents();
    }

    pio = pio0;

    uint offset_data = pio_add_program(pio, &data_bus_program);
    data_bus_sm = pio_claim_unused_sm(pio, true);
    data_bus_program_init(pio, data_bus_sm, offset_data);

    uint offset_addr = pio_add_program(pio, &addr_bus_program);
    addr_bus_sm = pio_claim_unused_sm(pio, true);
    addr_bus_program_init(pio, addr_bus_sm, offset_addr);

    uint offset_pindirs = pio_add_program(pio, &bus_dir_program);
    pindirs_bus_sm = pio_claim_unused_sm(pio, true);
    bus_dir_program_init(pio, pindirs_bus_sm, offset_pindirs);
}

#define DUMMY_BLINK
// #define MODIFY_DEFAULT_CLOCK

int main()
{
    // Default UART 115400
    stdio_init_all();
    printf("stdio init\n");
    if (cyw43_arch_init())
    {
    printf("cyw43 init fail\n");

        return -1;
    }
    printf("cyw43 init success\n");


#ifdef DUMMY_BLINK
    while (true)
    {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(50);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(50);
    }
#endif
#ifdef MODIFY_DEFAULT_CLOCK
    set_sys_clock_khz(150000, true);
#endif

    setup_pio();
    multicore_launch_core1(enter_bus_loop);

    DEBUG3("core1: init\n");
    // cyw43_arch_enable_sta_mode();
    const char *ap_name = "picow_test";
    const char *password = NULL;
    cyw43_arch_enable_ap_mode(ap_name, password, CYW43_AUTH_WPA2_AES_PSK);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    while (true)
    {
        if (status & STATUS_BUSY)
        {
            // DEBUG3("%d: enter handler\n", num);
            if (io_addr == IO_CMD && !io_read)
            {
                cmd_handler();
            }
            else if (io_addr == IO_DATA)
            {
                data_handler();
            }
            else
            {
                status |= STATUS_ERROR;
            }
            __dmb();
            status &= ~STATUS_BUSY;
        }
    }
}
