#include <stdio.h>
#include "flash.h"
#include "drv_spiflash.h"
#include "spif.h"
#include "ali_dfu_port.h"

#define LOG    printf

#define ADDR_GRAN_MASK     (0xFFF)
#define ADDR_ALIGN_UP(a)   (((a) + ADDR_GRAN_MASK) & ~ADDR_GRAN_MASK)

static unsigned short image_crc16 = 0;

/**
 * @brief 写flash上锁
 *
 * @param[in]  -
 *
 * @return -
 */
void lock_flash()
{
    hal_flash_enable_secure(HAL_PARTITION_OTA_TEMP, 0, 0);
}

/**
 * @brief 写flash解锁
 *
 * @param[in]  -
 *
 * @return -
 */
void unlock_flash_all()
{
    hal_flash_dis_secure(HAL_PARTITION_OTA_TEMP, 0, 0);
}

/**
 * @brief 镜像更新
 *
 * @param[in]  signature    暂时不使用
 * @param[in]  offset       当前buf代表的内容，从镜像bin文件从offset位置开始，比如为100，表示当前buffer是bin文件的第100字节开始
 * @param[in]  length       本次buffer的长度
 * @param[in]  buf          本次写入的具体内容
 *
 * @return 0:success, otherwise is failed
 */
int ali_dfu_image_update(short signature, int offset, int length, int *buf)
{
    int ret;
    hal_logic_partition_t *partition_info;
    uint32_t wr_idx = offset;
    uint8_t *wr_buf = buf;

    ///get OTA temporary partition information
    partition_info = hal_flash_get_info(HAL_PARTITION_OTA_TEMP);

    if(partition_info == NULL ||
       partition_info->partition_length < (offset+length))
    {
        LOG("The write range is over OTA temporary!\r\n");
        return -1;
    }

    /* For bootloader upgrade, we will reserve two sectors, then save the image */
    wr_idx += (SPIF_SECTOR_SIZE << 1);
    ret = hal_flash_write(HAL_PARTITION_OTA_TEMP, &wr_idx, (void *)wr_buf, length);
    if (ret < 0) {
        LOG("write flash error!!\r\n");
        return -1;
    }

    wr_idx = offset + (SPIF_SECTOR_SIZE << 1);
    ret = hal_flash_read(HAL_PARTITION_OTA_TEMP, &wr_idx, (void *)wr_buf, length);
    if (ret < 0) {
        LOG("read flash error!!\r\n");
        return -1;
    }

    if (offset == 0) {
        image_crc16 = util_crc16_ccitt((const uint8_t *)wr_buf, length, NULL);
    } else {
        image_crc16 = util_crc16_ccitt((const uint8_t *)wr_buf, length, &image_crc16);
    }

    //LOG("write ok!\n");
    return 0;
}

/**
 * @brief 写入flash之前和之后checksum计算
 *
 * @param[in]  image_id 暂时不使用
 * @param[in]  crc16_output 计算出来的crc返回给调用者
 *
 * @return 1:success 0:failed
 */
unsigned char dfu_check_checksum(short image_id, unsigned short *crc16_output)
{
    *crc16_output = image_crc16;
	return 1;
}

/**
 * @brief 升级结束后重启
 *
 * @param[in]  -
 * @return -
 * @说明: 比如在此函数里面call 切换镜像分区的业务逻辑
 */
void dfu_reboot()
{
	hal_reboot();
}

uint8_t get_program_image(void)
{
    return 0;
}

uint8_t change_program_image(uint8_t dfu_image)
{
    return 0;
}

int erase_dfu_flash(void)
{
    int ret;
    hal_logic_partition_t *partition_info;
    uint32_t offset = (SPIF_SECTOR_SIZE << 1);
    uint32_t length = 0;

    ///get OTA temporary partition information
    partition_info = hal_flash_get_info(HAL_PARTITION_OTA_TEMP);

    if(partition_info == NULL)
    {
        return -1;
    }
    length = partition_info->partition_length - offset;

    //LOG("Erase %x,%x!!\r\n", offset, length);

    /* For bootloader upgrade, we will reserve two sectors, then save the image */
    ret = hal_flash_erase(HAL_PARTITION_OTA_TEMP, offset, length);
    if (ret < 0) {
        LOG("Erase flash error!!\r\n");
        return -1;
    }
    return 0;
}

