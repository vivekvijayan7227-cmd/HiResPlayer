namespace HiResPlayer.Infrastructure.Native
{
    public record AudioDeviceItem(string Id, string FriendlyName, uint State, bool IsDefault);
}
