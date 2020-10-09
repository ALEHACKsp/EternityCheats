#pragma once

#include "memory.hpp"
#include "vectors.hpp"

constexpr const char* title = "R5Apex.exe";

DWORD32 g_pid = 7296;
DWORD64 g_base = 0x00007FF6ED2A0000;

typedef struct Bone
{
	uint8_t pad1[0xCC];
	float x;
	uint8_t pad2[0xC];
	float y;
	uint8_t pad3[0xC];
	float z;
}Bone;

class Entity
{
public:
	Entity(DWORD32 pid, DWORD64 p)
	{
		ptr = p;

		struct help { uint8_t data[0x3FF0]{ 0 }; };
		help temp = read<help>(pid, ptr);
		memcpy(buffer, temp.data, 0x3FF0);
	}

public:
	uint64_t ptr;
	uint8_t buffer[0x3FF0];

	Vector getPosition()
	{
		return *(Vector*)(buffer + 0x14c);
	}

	bool isDummy()
	{
		return *(int*)(buffer + 0x430) == 97;
	}

	bool isPlayer()
	{
		return *(uint64_t*)(buffer + 0x561) == 125780153691248;
	}

	
	bool isKnocked()
	{
		return *(int*)(buffer + 0x2610) > 0;
	}

	bool isAlive()
	{
		return *(int*)(buffer + 0x770) == 0;
	}

	float lastVisTime()
	{
		return *(float*)(buffer + 0x1A6C);
	}

	int getTeamId()
	{
		return *(int*)(buffer + 0x430);
	}

	int getHealth()
	{
		return *(int*)(buffer + 0x420);
	}

	int getShield()
	{
		return *(int*)(buffer + 0x170);
	}

	bool isGlowing()
	{
		return *(int*)(buffer + 0x350) == 7;
	}

	Vector getAbsVelocity()
	{
		return *(Vector*)(buffer + 0x140);
	}

	QAngle GetSwayAngles()
	{
		return *(QAngle*)(buffer + 0x24A0 - 0x10);
	}

	QAngle GetViewAngles()
	{
		return *(QAngle*)(buffer + 0x24A0);
	}

	Vector GetCamPos()
	{
		return *(Vector*)(buffer + 0x1E6C);
	}

	QAngle GetRecoil()
	{
		return *(QAngle*)(buffer + 0x23C8);
	}

	Vector GetViewAnglesV() {}

	void enableGlow(DWORD32 pid)
	{
		write<int>(pid, ptr + 0x262, 16256);
		write<int>(pid, ptr + 0x2c4, 1193322764);
		write<int>(pid, ptr + 0x350, 7);
		write<int>(pid, ptr + 0x360, 2);
	}

	void disableGlow(DWORD32 pid)
	{
		write<int>(pid, ptr + 0x262, 0);
		write<int>(pid, ptr + 0x2c4, 0);
		write<int>(pid, ptr + 0x350, 2);
		write<int>(pid, ptr + 0x360, 5);
	}

	void SetViewAngles(DWORD32 pid, Vector v)
	{
		write<Vector>(pid, ptr + 0x24A0, v);
	}

	Vector getBonePosition(int pid, int id)
	{
		Vector position = getPosition();
		uintptr_t boneArray = *(uintptr_t*)(buffer + 0xF18);
		Vector bone = Vector();
		uint32_t boneloc = (id * 0x30);
		Bone bo = read<Bone>(pid, boneArray + boneloc);
		bone.x = bo.x + position.x;
		bone.y = bo.y + position.y;
		bone.z = bo.z + position.z;
		return bone;
	}
	uint64_t Observing() {}
};

bool initialize()
{
	g_pid = get_process_id("r5apex.exe");
	if (g_pid == 0) return false;

	return true;
}

void glow_players(bool state = true)
{
	DWORD64 local_player_ptr = read<DWORD64>(g_pid, g_base + 0x1c5bcc8);
	if (local_player_ptr == 0) return;
	Entity local_player(g_pid, local_player_ptr);

	if (local_player.isAlive() == false) return;

	int team = local_player.getTeamId();
	if (team <= 0 || team > 50) return;

	uint64_t entitylist = g_base + 0x18ad3a8;

	for (int i = 0; i < 9000; i++)
	{
		uint64_t centity = read<uint64_t>(g_pid, entitylist + ((uint64_t)i << 5));
		if (centity == 0 || centity == local_player_ptr) continue;

		Entity e(g_pid, centity);
		if ((e.isDummy() || e.isPlayer()) && e.isAlive())
		{
			if (state)
				e.enableGlow(g_pid);
			else
				e.disableGlow(g_pid);
		}
	}
}

QAngle calc_angle(const Vector& src, const Vector& dst)
{
	QAngle angle = QAngle();
	QAngle delta = QAngle((src.x - dst.x), (src.y - dst.y), (src.z - dst.z));

	double hyp = sqrt(delta.x*delta.x + delta.y * delta.y);

	angle.x = atan(delta.z / hyp) * (180.0f / M_PI);
	angle.y = atan(delta.y / delta.x) * (180.0f / M_PI);
	angle.z = 0;
	if (delta.x >= 0.0) angle.y += 180.0f;

	return angle;
}

void clamp_angles(D3DXVECTOR3& vAngles)
{
	while (vAngles.y < -180.0f) vAngles.y += 360.0f;
	while (vAngles.y > 180.0f) vAngles.y -= 360.0f;

	if (vAngles.x < -89.0f) vAngles.x = 89.0f;
	if (vAngles.x > 89.0f) vAngles.x = 89.0f;

	vAngles.z = 0;
}

void normalize_angles(QAngle& angle)
{
	while (angle.x > 89.0f)
		angle.x -= 180.f;

	while (angle.x < -89.0f)
		angle.x += 180.f;

	while (angle.y > 180.f)
		angle.y -= 360.f;

	while (angle.y < -180.f)
		angle.y += 360.f;
}

double get_fov(const QAngle& viewAngle, const QAngle& aimAngle)
{
	QAngle delta = aimAngle - viewAngle;
	normalize_angles(delta);

	return sqrt(pow(delta.x, 2.0f) + pow(delta.y, 2.0f));
}

void get_aimbot_angle(Vector self_location, Vector player_location, QAngle& aim_angle)
{
	float x = self_location[0] - player_location[0];
	float y = self_location[1] - player_location[1];
	float z = self_location[2] - player_location[2] + 65.0f;

	const float pi = 3.1415f;
	aim_angle[0] = (float)atan(z / sqrt(x * x + y * y)) / pi * 180.f;
	aim_angle[1] = (float)atan(y / x);

	if (x >= 0.0f && y >= 0.0f) aim_angle[1] = aim_angle[1] / pi * 180.0f - 180.0f;
	else if (x < 0.0f && y >= 0.0f) aim_angle[1] = aim_angle[1] / pi * 180.0f;
	else if (x < 0.0f && y < 0.0f) aim_angle[1] = aim_angle[1] / pi * 180.0f;
	else if (x >= 0.0f && y < 0.0f) aim_angle[1] = aim_angle[1] / pi * 180.f + 180.0f;
}

bool get_the_fov(Entity& local_player, Entity& target, double& d, Vector& v)
{
	Vector LocalCamera = local_player.GetCamPos();
	Vector TargetBonePosition = target.getBonePosition(g_pid, 2);
	QAngle CalculatedAngles = calc_angle(LocalCamera, TargetBonePosition);
	QAngle ViewAngles = local_player.GetViewAngles();
	QAngle SwayAngles = local_player.GetSwayAngles();

	get_aimbot_angle(local_player.getBonePosition(g_pid, 8), TargetBonePosition, CalculatedAngles);

	//remove sway and recoil
	//CalculatedAngles -= SwayAngles - ViewAngles;
	//	CalculatedAngles -= local_player.GetRecoil();
	CalculatedAngles -= local_player.GetRecoil() /** 1.5f*/;

	normalize_angles(CalculatedAngles);

	QAngle Delta = CalculatedAngles - ViewAngles;
	double f = get_fov(SwayAngles, CalculatedAngles);
	if (d < f) return false;
	else d = f;

	normalize_angles(Delta);
	QAngle SmoothedAngles = ViewAngles + Delta;

	v.x = CalculatedAngles.x;
	v.y = CalculatedAngles.y;
	v.z = CalculatedAngles.z;

	return true;
}

float calculate_fov(Entity& from, Entity& target)
{
	QAngle ViewAngles = from.GetViewAngles();
	Vector LocalCamera = from.GetCamPos();
	Vector EntityPosition = target.getPosition();
	QAngle Angle = calc_angle(LocalCamera, EntityPosition);
	return get_fov(ViewAngles, Angle);
}

bool calculate_best_bone_aim(Entity& from, Entity& target, float max_fov, Vector& v)
{
	Vector LocalCamera = from.GetCamPos();
	Vector TargetBonePosition = target.getBonePosition(g_pid, 8);
	QAngle CalculatedAngles = calc_angle(LocalCamera, TargetBonePosition);
	QAngle ViewAngles = from.GetViewAngles();
	QAngle SwayAngles = from.GetSwayAngles();

	//remove sway and recoil
	//CalculatedAngles -= SwayAngles - ViewAngles;

	normalize_angles(CalculatedAngles);
	QAngle Delta = CalculatedAngles - ViewAngles;
	double fov = get_fov(SwayAngles, CalculatedAngles);
	if (fov > max_fov) return false;

	normalize_angles(Delta);

	QAngle SmoothedAngles = ViewAngles + Delta;
	v.x = SmoothedAngles.x;
	v.y = SmoothedAngles.y;
	v.z = SmoothedAngles.z;

	return true;
}
bool start_aimbot(Entity& local_player, Entity& target)
{
	Vector LocalCamera = local_player.GetCamPos();
	Vector TargetBonePosition = target.getBonePosition(g_pid, 8);
	QAngle CalculatedAngles = calc_angle(LocalCamera, TargetBonePosition);
	QAngle ViewAngles = local_player.GetViewAngles();
	QAngle SwayAngles = local_player.GetSwayAngles();

	//remove sway and recoil
	CalculatedAngles -= SwayAngles - ViewAngles;

	normalize_angles(CalculatedAngles);

	QAngle Delta = CalculatedAngles - ViewAngles;
	double fov = get_fov(SwayAngles, CalculatedAngles);

	if (fov > 50.0f) return false;

	normalize_angles(Delta);
	QAngle SmoothedAngles = ViewAngles + Delta;
	Vector r{ };
	r.x = SmoothedAngles.x;
	r.y = SmoothedAngles.y;
	r.z = SmoothedAngles.z;
	local_player.SetViewAngles(g_pid, r);

	return true;
}

void aimbot_players()
{
	DWORD64 local_player_ptr = read<DWORD64>(g_pid, g_base + 0x1c5bcc8);
	if (local_player_ptr == 0) return;
	Entity local_player(g_pid, local_player_ptr);

	if (local_player.isAlive() == false) return;

	Vector LocalPlayerPosition = local_player.getPosition();

	double min_fov = 30.0f;
	bool state = false;
	Vector v;

	uint64_t entitylist = g_base + 0x18ad3a8;
	for (int i = 0; i < 9000; i++)
	{
		uint64_t centity = read<uint64_t>(g_pid, entitylist + ((uint64_t)i << 5));
		if (centity == 0 || centity == local_player_ptr) continue;

		Entity e(g_pid, centity);

		if ((e.isPlayer() || e.isDummy()) && e.isAlive() && local_player.getTeamId() != e.getTeamId())
		{
			float distance = LocalPlayerPosition.DistTo(e.getPosition());
			if (distance > 2000.0f) continue;

			if (get_the_fov(local_player, e, min_fov, v))
				state = true;
		}
	}

	if (state)
		local_player.SetViewAngles(g_pid, v);
}