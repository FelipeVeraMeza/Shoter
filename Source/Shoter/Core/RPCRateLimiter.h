#pragma once

#include "CoreMinimal.h"

/**
 * Limitador de frecuencia de RPCs por cliente (RNF-3.5).
 *
 * Un cliente modificado puede invocar una RPC de servidor en bucle. Aunque
 * cada llamada se valide, atenderlas todas ya consume tiempo del servidor: el
 * ataque no necesita que la trampa funcione, solo que el servidor se ocupe.
 *
 * Usa una ventana deslizante simple en vez de un cubo de tokens: el caso a
 * frenar es una ráfaga sostenida, y contar llamadas por ventana basta para
 * eso sin mantener estado por cada RPC.
 *
 * No es un tipo UObject a propósito: se incrusta por valor en el actor que
 * recibe la RPC, sin coste de replicación ni de recolección de basura.
 */
struct FRPCRateLimiter
{
	/** Llamadas permitidas dentro de la ventana. */
	int32 MaxCalls = 30;

	/** Duración de la ventana en segundos. */
	float WindowSeconds = 1.f;

	FRPCRateLimiter() = default;

	FRPCRateLimiter(int32 InMaxCalls, float InWindowSeconds)
		: MaxCalls(InMaxCalls)
		, WindowSeconds(InWindowSeconds)
	{
	}

	/**
	 * Registra una llamada y dice si debe atenderse.
	 *
	 * Devuelve false cuando el cliente supera el límite; quien llama descarta
	 * la RPC en silencio. Responder con un error daría al atacante una señal
	 * de dónde está el umbral.
	 */
	bool AllowCall(float CurrentWorldTime)
	{
		// Ventana cumplida: empieza el recuento de nuevo.
		if (CurrentWorldTime - WindowStartTime >= WindowSeconds)
		{
			WindowStartTime = CurrentWorldTime;
			CallsInWindow = 0;
		}

		++CallsInWindow;

		if (CallsInWindow > MaxCalls)
		{
			++RejectedCalls;
			return false;
		}

		return true;
	}

	/** Llamadas rechazadas desde el inicio. Útil para telemetría (RF-13.4). */
	int32 GetRejectedCalls() const { return RejectedCalls; }

	void Reset()
	{
		WindowStartTime = 0.f;
		CallsInWindow = 0;
	}

private:
	float WindowStartTime = 0.f;
	int32 CallsInWindow = 0;
	int32 RejectedCalls = 0;
};
